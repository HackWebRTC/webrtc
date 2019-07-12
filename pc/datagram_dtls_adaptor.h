/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DATAGRAM_DTLS_ADAPTOR_H_
#define PC_DATAGRAM_DTLS_ADAPTOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/crypto/crypto_options.h"
#include "api/datagram_transport_interface.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread_checker.h"

namespace cricket {

constexpr int kDatagramDtlsAdaptorComponent = -1;

// DTLS wrapper around DatagramTransportInterface.
// Does not encrypt.
// Owns Datagram and Ice transports.
class DatagramDtlsAdaptor : public DtlsTransportInternal,
                            public webrtc::DatagramSinkInterface,
                            public webrtc::MediaTransportStateCallback {
 public:
  // TODO(sukhanov): Taking crypto options, because DtlsTransportInternal
  // has a virtual getter crypto_options(). Consider removing getter and
  // removing crypto_options from DatagramDtlsAdaptor.
  DatagramDtlsAdaptor(
      const std::vector<webrtc::RtpExtension>& rtp_header_extensions,
      IceTransportInternal* ice_transport,
      webrtc::DatagramTransportInterface* datagram_transport,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::RtcEventLog* event_log);

  ~DatagramDtlsAdaptor() override;

  // Connects to ICE transport callbacks.
  void ConnectToIceTransport();

  // =====================================================
  // Overrides for webrtc::DatagramTransportSinkInterface
  // and MediaTransportStateCallback
  // =====================================================
  void OnDatagramReceived(rtc::ArrayView<const uint8_t> data) override;

  void OnDatagramSent(webrtc::DatagramId datagram_id) override;

  void OnDatagramAcked(const webrtc::DatagramAck& ack) override;

  void OnDatagramLost(webrtc::DatagramId datagram_id) override;

  void OnStateChanged(webrtc::MediaTransportState state) override;

  // =====================================================
  // DtlsTransportInternal overrides
  // =====================================================
  const webrtc::CryptoOptions& crypto_options() const override;
  DtlsTransportState dtls_state() const override;
  int component() const override;
  bool IsDtlsActive() const override;
  bool GetDtlsRole(rtc::SSLRole* role) const override;
  bool SetDtlsRole(rtc::SSLRole role) override;
  bool GetSrtpCryptoSuite(int* cipher) override;
  bool GetSslCipherSuite(int* cipher) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain() const override;
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;
  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;
  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override;
  IceTransportInternal* ice_transport() override;

  const std::string& transport_name() const override;
  bool writable() const override;
  bool receiving() const override;

 private:
  // RTP/RTCP packet info stored for each sent packet.
  struct SentPacketInfo {
    // RTP packet info with ssrc and transport sequence number.
    SentPacketInfo(int64_t packet_id,
                   uint32_t ssrc,
                   uint16_t transport_sequence_number)
        : ssrc(ssrc),
          transport_sequence_number(transport_sequence_number),
          packet_id(packet_id) {}

    // Packet info without SSRC and transport sequence number used for RTCP
    // packets, RTP packets when transport sequence number is not provided or
    // when feedback translation is disabled.
    explicit SentPacketInfo(int64_t packet_id) : packet_id(packet_id) {}

    SentPacketInfo() = default;

    absl::optional<uint32_t> ssrc;

    // Transport sequence number (if it was provided in outgoing RTP packet).
    // It is used to re-create RTCP feedback packets from datagram ACKs.
    absl::optional<uint16_t> transport_sequence_number;

    // Packet id from rtc::PacketOptions. It is required to propagage sent
    // notification up the stack (SignalSentPacket).
    int64_t packet_id = 0;
  };

  // Finds SentPacketInfo for given |datagram_id| and removes map entry.
  // Returns false if entry was not found.
  bool GetAndRemoveSentPacketInfo(webrtc::DatagramId datagram_id,
                                  SentPacketInfo* sent_packet_info);

  // Sends datagram to datagram_transport.
  int SendDatagram(rtc::ArrayView<const uint8_t> data,
                   webrtc::DatagramId datagram_id);

  void set_receiving(bool receiving);
  void set_writable(bool writable);
  void set_dtls_state(DtlsTransportState state);

  // Forwards incoming packet up the stack.
  void PropagateReadPacket(rtc::ArrayView<const uint8_t> data,
                           const int64_t& packet_time_us);

  // Signals SentPacket notification.
  void PropagateOnSentNotification(const rtc::SentPacket& sent_packet);

  // Listens to read packet notifications from ICE (only used in bypass mode).
  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t size,
                    const int64_t& packet_time_us,
                    int flags);

  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);
  void OnReceivingState(rtc::PacketTransportInternal* transport);

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetError() override;
  void OnSentPacket(rtc::PacketTransportInternal* transport,
                    const rtc::SentPacket& sent_packet);

  rtc::ThreadChecker thread_checker_;
  webrtc::CryptoOptions crypto_options_;
  IceTransportInternal* ice_transport_;

  webrtc::DatagramTransportInterface* datagram_transport_;

  // Current ICE writable state. Must be modified by calling set_ice_writable(),
  // which propagates change notifications.
  bool writable_ = false;

  // Current receiving state. Must be modified by calling set_receiving(), which
  // propagates change notifications.
  bool receiving_ = false;

  // Current DTLS state. Must be modified by calling set_dtls_state(), which
  // propagates change notifications.
  DtlsTransportState dtls_state_ = DTLS_TRANSPORT_NEW;

  webrtc::RtcEventLog* const event_log_;

  // Extension map for parsing transport sequence numbers.
  webrtc::RtpHeaderExtensionMap rtp_header_extension_map_;

  // Keeps information about sent RTP packet until they are Acked or Lost.
  std::map<webrtc::DatagramId, SentPacketInfo> sent_rtp_packet_map_;

  // Current datagram_id, incremented after each sent RTP packets.
  // Datagram id is passed to datagram transport when we send datagram and we
  // get it back in notifications about Sent, Acked and Lost datagrams.
  int64_t current_datagram_id_ = 0;

  // TODO(sukhanov): Previous nonzero timestamp is required for workaround for
  // zero timestamps received, which sometimes are received from datagram
  // transport. Investigate if we can eliminate zero timestamps.
  int64_t previous_nonzero_timestamp_us_ = 0;

  // Disable datagram to RTCP feedback translation and enable RTCP feedback
  // loop (note that having both RTCP and datagram feedback loops is
  // inefficient, but can be useful in tests and experiments).
  const bool disable_datagram_to_rtcp_feeback_translation_;
};

}  // namespace cricket

#endif  // PC_DATAGRAM_DTLS_ADAPTOR_H_
