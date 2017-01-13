/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/flexfec_receive_stream_impl.h"

#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

std::string FlexfecReceiveStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "FlexfecReceiveStream stats: " << time_ms
     << ", {flexfec_bitrate_bps: " << flexfec_bitrate_bps << "}";
  return ss.str();
}

std::string FlexfecReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{payload_type: " << payload_type;
  ss << ", remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", protected_media_ssrcs: [";
  size_t i = 0;
  for (; i + 1 < protected_media_ssrcs.size(); ++i)
    ss << protected_media_ssrcs[i] << ", ";
  if (!protected_media_ssrcs.empty())
    ss << protected_media_ssrcs[i];
  ss << "], transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", rtp_header_extensions: [";
  i = 0;
  for (; i + 1 < rtp_header_extensions.size(); ++i)
    ss << rtp_header_extensions[i].ToString() << ", ";
  if (!rtp_header_extensions.empty())
    ss << rtp_header_extensions[i].ToString();
  ss << "]}";
  return ss.str();
}

bool FlexfecReceiveStream::Config::IsCompleteAndEnabled() const {
  // Check if FlexFEC is enabled.
  if (payload_type < 0)
    return false;
  // Do we have the necessary SSRC information?
  if (remote_ssrc == 0)
    return false;
  // TODO(brandtr): Update this check when we support multistream protection.
  if (protected_media_ssrcs.size() != 1u)
    return false;
  return true;
}

namespace {

// TODO(brandtr): Update this function when we support multistream protection.
std::unique_ptr<FlexfecReceiver> MaybeCreateFlexfecReceiver(
    const FlexfecReceiveStream::Config& config,
    RecoveredPacketReceiver* recovered_packet_receiver) {
  if (config.payload_type < 0) {
    LOG(LS_WARNING) << "Invalid FlexFEC payload type given. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }
  RTC_DCHECK_GE(config.payload_type, 0);
  RTC_DCHECK_LE(config.payload_type, 127);
  if (config.remote_ssrc == 0) {
    LOG(LS_WARNING) << "Invalid FlexFEC SSRC given. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }
  if (config.protected_media_ssrcs.empty()) {
    LOG(LS_WARNING) << "No protected media SSRC supplied. "
                    << "This FlexfecReceiveStream will therefore be useless.";
    return nullptr;
  }

  if (config.protected_media_ssrcs.size() > 1) {
    LOG(LS_WARNING)
        << "The supplied FlexfecConfig contained multiple protected "
           "media streams, but our implementation currently only "
           "supports protecting a single media stream. "
           "To avoid confusion, disabling FlexFEC completely.";
    return nullptr;
  }
  RTC_DCHECK_EQ(1U, config.protected_media_ssrcs.size());
  return std::unique_ptr<FlexfecReceiver>(
      new FlexfecReceiver(config.remote_ssrc, config.protected_media_ssrcs[0],
                          recovered_packet_receiver));
}

}  // namespace

FlexfecReceiveStreamImpl::FlexfecReceiveStreamImpl(
    const Config& config,
    RecoveredPacketReceiver* recovered_packet_receiver)
    : started_(false),
      config_(config),
      receiver_(
          MaybeCreateFlexfecReceiver(config_, recovered_packet_receiver)) {
  LOG(LS_INFO) << "FlexfecReceiveStreamImpl: " << config_.ToString();
}

FlexfecReceiveStreamImpl::~FlexfecReceiveStreamImpl() {
  LOG(LS_INFO) << "~FlexfecReceiveStreamImpl: " << config_.ToString();
  Stop();
}

bool FlexfecReceiveStreamImpl::AddAndProcessReceivedPacket(
    RtpPacketReceived packet) {
  {
    rtc::CritScope cs(&crit_);
    if (!started_)
      return false;
  }
  if (!receiver_)
    return false;
  return receiver_->AddAndProcessReceivedPacket(std::move(packet));
}

void FlexfecReceiveStreamImpl::Start() {
  rtc::CritScope cs(&crit_);
  started_ = true;
}

void FlexfecReceiveStreamImpl::Stop() {
  rtc::CritScope cs(&crit_);
  started_ = false;
}

// TODO(brandtr): Implement this member function when we have designed the
// stats for FlexFEC.
FlexfecReceiveStreamImpl::Stats FlexfecReceiveStreamImpl::GetStats() const {
  return FlexfecReceiveStream::Stats();
}

}  // namespace webrtc
