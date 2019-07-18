/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/datagram_dtls_adaptor.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/rtc_error.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"

#ifdef BYPASS_DATAGRAM_DTLS_TEST_ONLY
// Send unencrypted packets directly to ICE, bypassing datagtram
// transport. Use in tests only.
constexpr bool kBypassDatagramDtlsTestOnly = true;
#else
constexpr bool kBypassDatagramDtlsTestOnly = false;
#endif

namespace cricket {

namespace {

// Field trials.
// Disable datagram to RTCP feedback translation and enable RTCP feedback loop
// on top of datagram feedback loop. Note that two
// feedback loops add unneccesary overhead, so it's preferable to use feedback
// loop provided by datagram transport and convert datagram ACKs to RTCP ACKs,
// but enabling RTCP feedback loop may be useful in tests and experiments.
const char kDisableDatagramToRtcpFeebackTranslationFieldTrial[] =
    "WebRTC-kDisableDatagramToRtcpFeebackTranslation";

}  // namespace

// Maximum packet size of RTCP feedback packet for allocation. We re-create RTCP
// feedback packets when we get ACK notifications from datagram transport. Our
// rtcp feedback packets contain only 1 ACK, so they are much smaller than 1250.
constexpr size_t kMaxRtcpFeedbackPacketSize = 1250;

DatagramDtlsAdaptor::DatagramDtlsAdaptor(
    const std::vector<webrtc::RtpExtension>& rtp_header_extensions,
    IceTransportInternal* ice_transport,
    webrtc::DatagramTransportInterface* datagram_transport,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::RtcEventLog* event_log)
    : crypto_options_(crypto_options),
      ice_transport_(ice_transport),
      datagram_transport_(datagram_transport),
      event_log_(event_log),
      disable_datagram_to_rtcp_feeback_translation_(
          webrtc::field_trial::IsEnabled(
              kDisableDatagramToRtcpFeebackTranslationFieldTrial)) {
  // Save extension map for parsing RTP packets (we only need transport
  // sequence numbers).
  const webrtc::RtpExtension* transport_sequence_number_extension =
      webrtc::RtpExtension::FindHeaderExtensionByUri(
          rtp_header_extensions, webrtc::TransportSequenceNumber::kUri);

  if (transport_sequence_number_extension != nullptr) {
    rtp_header_extension_map_.Register<webrtc::TransportSequenceNumber>(
        transport_sequence_number_extension->id);
  } else {
    RTC_LOG(LS_ERROR) << "Transport sequence numbers are not supported in "
                         "datagram transport connection";
  }

  RTC_DCHECK(ice_transport_);
  RTC_DCHECK(datagram_transport_);
  ConnectToIceTransport();
}

void DatagramDtlsAdaptor::ConnectToIceTransport() {
  ice_transport_->SignalWritableState.connect(
      this, &DatagramDtlsAdaptor::OnWritableState);
  ice_transport_->SignalReadyToSend.connect(
      this, &DatagramDtlsAdaptor::OnReadyToSend);
  ice_transport_->SignalReceivingState.connect(
      this, &DatagramDtlsAdaptor::OnReceivingState);
  // Datagram transport does not propagate network route change.
  ice_transport_->SignalNetworkRouteChanged.connect(
      this, &DatagramDtlsAdaptor::OnNetworkRouteChanged);
  if (kBypassDatagramDtlsTestOnly) {
    // In bypass mode we have to subscribe to ICE read and sent events.
    // Test only case to use ICE directly instead of data transport.
    ice_transport_->SignalReadPacket.connect(
        this, &DatagramDtlsAdaptor::OnReadPacket);
    ice_transport_->SignalSentPacket.connect(
        this, &DatagramDtlsAdaptor::OnSentPacket);
  } else {
    // Subscribe to Data Transport read packets.
    datagram_transport_->SetDatagramSink(this);
    datagram_transport_->SetTransportStateCallback(this);
  }
}

DatagramDtlsAdaptor::~DatagramDtlsAdaptor() {
  // Unsubscribe from Datagram Transport dinks.
  datagram_transport_->SetDatagramSink(nullptr);
  datagram_transport_->SetTransportStateCallback(nullptr);
}

const webrtc::CryptoOptions& DatagramDtlsAdaptor::crypto_options() const {
  return crypto_options_;
}

int DatagramDtlsAdaptor::SendPacket(const char* data,
                                    size_t len,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // TODO(sukhanov): Handle options and flags.
  if (kBypassDatagramDtlsTestOnly) {
    // In bypass mode sent directly to ICE.
    return ice_transport_->SendPacket(data, len, options);
  }

  // Assign and increment datagram_id.
  const webrtc::DatagramId datagram_id = current_datagram_id_++;

  rtc::ArrayView<const uint8_t> original_data(
      reinterpret_cast<const uint8_t*>(data), len);

  // Send as is (without extracting transport sequence number) for
  //    - All RTCP packets, because they do not have transport sequence number.
  //    - RTP packets if we are not doing datagram => RTCP feedback translation.
  if (disable_datagram_to_rtcp_feeback_translation_ ||
      webrtc::RtpHeaderParser::IsRtcp(original_data.data(),
                                      original_data.size())) {
    // Even if we are not extracting transport sequence number we need to
    // propagate "Sent" notification for both RTP and RTCP packets. For this
    // reason we need save options.packet_id in packet map.
    sent_rtp_packet_map_[datagram_id] = SentPacketInfo(options.packet_id);

    return SendDatagram(original_data, datagram_id);
  }

  // Parse RTP packet.
  webrtc::RtpPacket rtp_packet(&rtp_header_extension_map_);
  if (!rtp_packet.Parse(original_data)) {
    RTC_NOTREACHED() << "Failed to parse outgoing RtpPacket, len=" << len
                     << ", options.packet_id=" << options.packet_id;
    return -1;
  }

  // Try to get transport sequence number.
  uint16_t transport_senquence_number;
  if (!rtp_packet.GetExtension<webrtc::TransportSequenceNumber>(
          &transport_senquence_number)) {
    // Save packet info without transport sequence number.
    sent_rtp_packet_map_[datagram_id] = SentPacketInfo(options.packet_id);

    RTC_LOG(LS_VERBOSE)
        << "Sending rtp packet without transport sequence number, packet="
        << rtp_packet.ToString();

    return SendDatagram(original_data, datagram_id);
  }

  // Save packet info with sequence number and ssrc so we could reconstruct
  // RTCP feedback packet when we receive datagram ACK.
  sent_rtp_packet_map_[datagram_id] = SentPacketInfo(
      options.packet_id, rtp_packet.Ssrc(), transport_senquence_number);

  // Since datagram transport provides feedback and timestamps, we do not need
  // to send transport sequence number, so we remove it from RTP packet. Later
  // when we get Ack for sent datagram, we will re-create RTCP feedback packet.
  if (!rtp_packet.RemoveExtension(webrtc::TransportSequenceNumber::kId)) {
    RTC_NOTREACHED() << "Failed to remove transport sequence number, packet="
                     << rtp_packet.ToString();
    return -1;
  }

  RTC_LOG(LS_VERBOSE) << "Removed transport_senquence_number="
                      << transport_senquence_number
                      << " from packet=" << rtp_packet.ToString()
                      << ", saved bytes=" << len - rtp_packet.size();

  return SendDatagram(
      rtc::ArrayView<const uint8_t>(rtp_packet.data(), rtp_packet.size()),
      datagram_id);
}

int DatagramDtlsAdaptor::SendDatagram(rtc::ArrayView<const uint8_t> data,
                                      webrtc::DatagramId datagram_id) {
  webrtc::RTCError error = datagram_transport_->SendDatagram(data, datagram_id);
  return (error.ok() ? data.size() : -1);
}

void DatagramDtlsAdaptor::OnReadPacket(rtc::PacketTransportInternal* transport,
                                       const char* data,
                                       size_t size,
                                       const int64_t& packet_time_us,
                                       int flags) {
  // Only used in bypass mode.
  RTC_DCHECK(kBypassDatagramDtlsTestOnly);

  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK_EQ(transport, ice_transport_);
  RTC_DCHECK(flags == 0);

  PropagateReadPacket(
      rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data), size),
      packet_time_us);
}

void DatagramDtlsAdaptor::OnDatagramReceived(
    rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!kBypassDatagramDtlsTestOnly);

  // TODO(sukhanov): I am not filling out time, but on my video quality
  // test in WebRTC the time was not set either and higher layers of the stack
  // overwrite -1 with current current rtc time. Leaveing comment for now to
  // make sure it works as expected.
  int64_t packet_time_us = -1;

  PropagateReadPacket(data, packet_time_us);
}

void DatagramDtlsAdaptor::OnDatagramSent(webrtc::DatagramId datagram_id) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // Find packet_id and propagate OnPacketSent notification.
  const auto& it = sent_rtp_packet_map_.find(datagram_id);
  if (it == sent_rtp_packet_map_.end()) {
    RTC_NOTREACHED() << "Did not find sent packet info for sent datagram_id="
                     << datagram_id;
    return;
  }

  // Also see how DatagramDtlsAdaptor::OnSentPacket handles OnSentPacket
  // notification from ICE in bypass mode.
  rtc::SentPacket sent_packet(/*packet_id=*/it->second.packet_id,
                              rtc::TimeMillis());

  PropagateOnSentNotification(sent_packet);
}

bool DatagramDtlsAdaptor::GetAndRemoveSentPacketInfo(
    webrtc::DatagramId datagram_id,
    SentPacketInfo* sent_packet_info) {
  RTC_CHECK(sent_packet_info != nullptr);

  const auto& it = sent_rtp_packet_map_.find(datagram_id);
  if (it == sent_rtp_packet_map_.end()) {
    return false;
  }

  *sent_packet_info = it->second;
  sent_rtp_packet_map_.erase(it);
  return true;
}

void DatagramDtlsAdaptor::OnDatagramAcked(const webrtc::DatagramAck& ack) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  SentPacketInfo sent_packet_info;
  if (!GetAndRemoveSentPacketInfo(ack.datagram_id, &sent_packet_info)) {
    // TODO(sukhanov): If OnDatagramAck() can come after OnDatagramLost(),
    // datagram_id is already deleted and we may need to relax the CHECK below.
    // It's probably OK to ignore such datagrams, because it's been a few RTTs
    // anyway since they were sent.
    RTC_NOTREACHED() << "Did not find sent packet info for datagram_id="
                     << ack.datagram_id;
    return;
  }

  RTC_LOG(LS_VERBOSE) << "Datagram acked, ack.datagram_id=" << ack.datagram_id
                      << ", sent_packet_info.packet_id="
                      << sent_packet_info.packet_id
                      << ", sent_packet_info.transport_sequence_number="
                      << sent_packet_info.transport_sequence_number.value_or(-1)
                      << ", sent_packet_info.ssrc="
                      << sent_packet_info.ssrc.value_or(-1)
                      << ", receive_timestamp_ms="
                      << ack.receive_timestamp.ms();

  // If transport sequence number was not present in RTP packet, we do not need
  // to propagate RTCP feedback.
  if (!sent_packet_info.transport_sequence_number) {
    return;
  }

  // TODO(sukhanov): We noticed that datagram transport implementations can
  // return zero timestamps in the middle of the call. This is workaround to
  // avoid propagating zero timestamps, but we need to understand why we have
  // them in the first place.
  int64_t receive_timestamp_us = ack.receive_timestamp.us();

  if (receive_timestamp_us == 0) {
    receive_timestamp_us = previous_nonzero_timestamp_us_;
  } else {
    previous_nonzero_timestamp_us_ = receive_timestamp_us;
  }

  // Ssrc must be provided in packet info if transport sequence number is set,
  // which is guaranteed by SentPacketInfo constructor.
  RTC_CHECK(sent_packet_info.ssrc);

  // Recreate RTCP feedback packet.
  webrtc::rtcp::TransportFeedback feedback_packet;
  feedback_packet.SetMediaSsrc(*sent_packet_info.ssrc);

  const uint16_t transport_sequence_number =
      sent_packet_info.transport_sequence_number.value();

  feedback_packet.SetBase(transport_sequence_number, receive_timestamp_us);
  feedback_packet.AddReceivedPacket(transport_sequence_number,
                                    receive_timestamp_us);

  rtc::Buffer buffer(kMaxRtcpFeedbackPacketSize);
  size_t index = 0;
  if (!feedback_packet.Create(buffer.data(), &index, buffer.capacity(),
                              nullptr)) {
    RTC_NOTREACHED() << "Failed to create RTCP feedback packet";
    return;
  }

  RTC_CHECK_GT(index, 0);
  RTC_CHECK_LE(index, kMaxRtcpFeedbackPacketSize);

  // Propagage created RTCP packet as normal incoming packet.
  buffer.SetSize(index);
  PropagateReadPacket(buffer, /*packet_time_us=*/-1);
}

void DatagramDtlsAdaptor::OnDatagramLost(webrtc::DatagramId datagram_id) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  RTC_LOG(LS_INFO) << "Datagram lost, datagram_id=" << datagram_id;

  SentPacketInfo sent_packet_info;
  if (!GetAndRemoveSentPacketInfo(datagram_id, &sent_packet_info)) {
    RTC_NOTREACHED() << "Did not find sent packet info for lost datagram_id="
                     << datagram_id;
  }
}

void DatagramDtlsAdaptor::OnSentPacket(rtc::PacketTransportInternal* transport,
                                       const rtc::SentPacket& sent_packet) {
  // Only used in bypass mode.
  RTC_DCHECK(kBypassDatagramDtlsTestOnly);
  RTC_DCHECK_RUN_ON(&thread_checker_);

  PropagateOnSentNotification(sent_packet);
}

void DatagramDtlsAdaptor::PropagateOnSentNotification(
    const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalSentPacket(this, sent_packet);
}

void DatagramDtlsAdaptor::PropagateReadPacket(
    rtc::ArrayView<const uint8_t> data,
    const int64_t& packet_time_us) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalReadPacket(this, reinterpret_cast<const char*>(data.data()),
                   data.size(), packet_time_us, /*flags=*/0);
}

int DatagramDtlsAdaptor::component() const {
  return kDatagramDtlsAdaptorComponent;
}
bool DatagramDtlsAdaptor::IsDtlsActive() const {
  return false;
}
bool DatagramDtlsAdaptor::GetDtlsRole(rtc::SSLRole* role) const {
  return false;
}
bool DatagramDtlsAdaptor::SetDtlsRole(rtc::SSLRole role) {
  return false;
}
bool DatagramDtlsAdaptor::GetSrtpCryptoSuite(int* cipher) {
  return false;
}
bool DatagramDtlsAdaptor::GetSslCipherSuite(int* cipher) {
  return false;
}

rtc::scoped_refptr<rtc::RTCCertificate>
DatagramDtlsAdaptor::GetLocalCertificate() const {
  return nullptr;
}

bool DatagramDtlsAdaptor::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return false;
}

std::unique_ptr<rtc::SSLCertChain> DatagramDtlsAdaptor::GetRemoteSSLCertChain()
    const {
  return nullptr;
}

bool DatagramDtlsAdaptor::ExportKeyingMaterial(const std::string& label,
                                               const uint8_t* context,
                                               size_t context_len,
                                               bool use_context,
                                               uint8_t* result,
                                               size_t result_len) {
  return false;
}

bool DatagramDtlsAdaptor::SetRemoteFingerprint(const std::string& digest_alg,
                                               const uint8_t* digest,
                                               size_t digest_len) {
  // TODO(sukhanov): We probably should not called with fingerptints in
  // datagram scenario, but we may need to change code up the stack before
  // we can return false or DCHECK.
  return true;
}

bool DatagramDtlsAdaptor::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  // TODO(sukhanov): We may be able to return false and/or DCHECK that we
  // are not called if datagram transport is used, but we need to change
  // integration before we can do it.
  return true;
}

IceTransportInternal* DatagramDtlsAdaptor::ice_transport() {
  return ice_transport_;
}

// Similar implementaton as in p2p/base/dtls_transport.cc.
void DatagramDtlsAdaptor::OnReadyToSend(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void DatagramDtlsAdaptor::OnWritableState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_VERBOSE) << "ice_transport writable state changed to "
                      << ice_transport_->writable();

  if (kBypassDatagramDtlsTestOnly) {
    // Note: SignalWritableState fired by set_writable.
    set_writable(ice_transport_->writable());
    return;
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      break;
    case DTLS_TRANSPORT_CONNECTED:
      // Note: SignalWritableState fired by set_writable.
      // Do we also need set_receiving(ice_transport_->receiving()) here now, in
      // case we lose that signal before "DTLS" connects?
      // DtlsTransport::OnWritableState does not set_receiving in a similar
      // case, so leaving it out for the time being, but it would be good to
      // understand why.
      set_writable(ice_transport_->writable());
      break;
    case DTLS_TRANSPORT_CONNECTING:
      // Do nothing.
      break;
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // Should not happen. Do nothing.
      break;
  }
}

void DatagramDtlsAdaptor::OnStateChanged(webrtc::MediaTransportState state) {
  // Convert MediaTransportState to DTLS state.
  switch (state) {
    case webrtc::MediaTransportState::kPending:
      set_dtls_state(DTLS_TRANSPORT_CONNECTING);
      break;

    case webrtc::MediaTransportState::kWritable:
      // Since we do not set writable state until datagram transport is
      // connected, we need to call set_writable first.
      set_writable(ice_transport_->writable());
      set_dtls_state(DTLS_TRANSPORT_CONNECTED);
      break;

    case webrtc::MediaTransportState::kClosed:
      set_dtls_state(DTLS_TRANSPORT_CLOSED);
      break;
  }
}

DtlsTransportState DatagramDtlsAdaptor::dtls_state() const {
  return dtls_state_;
}

const std::string& DatagramDtlsAdaptor::transport_name() const {
  return ice_transport_->transport_name();
}

bool DatagramDtlsAdaptor::writable() const {
  // NOTE that even if ice is writable, writable_ maybe false, because we
  // propagte writable only after DTLS is connect (this is consistent with
  // implementation in dtls_transport.cc).
  return writable_;
}

bool DatagramDtlsAdaptor::receiving() const {
  return receiving_;
}

int DatagramDtlsAdaptor::SetOption(rtc::Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

int DatagramDtlsAdaptor::GetError() {
  return ice_transport_->GetError();
}

void DatagramDtlsAdaptor::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

void DatagramDtlsAdaptor::OnReceivingState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_VERBOSE) << "ice_transport receiving state changed to "
                      << ice_transport_->receiving();

  if (kBypassDatagramDtlsTestOnly || dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    // Note: SignalReceivingState fired by set_receiving.
    set_receiving(ice_transport_->receiving());
  }
}

void DatagramDtlsAdaptor::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

// Similar implementaton as in p2p/base/dtls_transport.cc.
void DatagramDtlsAdaptor::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  if (event_log_) {
    event_log_->Log(
        absl::make_unique<webrtc::RtcEventDtlsWritableState>(writable));
  }
  RTC_LOG(LS_VERBOSE) << "set_writable to: " << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

// Similar implementaton as in p2p/base/dtls_transport.cc.
void DatagramDtlsAdaptor::set_dtls_state(DtlsTransportState state) {
  if (dtls_state_ == state) {
    return;
  }
  if (event_log_) {
    event_log_->Log(absl::make_unique<webrtc::RtcEventDtlsTransportState>(
        ConvertDtlsTransportState(state)));
  }
  RTC_LOG(LS_VERBOSE) << "set_dtls_state from:" << dtls_state_ << " to "
                      << state;
  dtls_state_ = state;
  SignalDtlsState(this, state);
}

}  // namespace cricket
