/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/audio_receive_stream.h"

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
std::string AudioReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << '}';
  return ss.str();
}

namespace internal {
AudioReceiveStream::AudioReceiveStream(
      RemoteBitrateEstimator* remote_bitrate_estimator,
      const webrtc::AudioReceiveStream::Config& config)
    : remote_bitrate_estimator_(remote_bitrate_estimator),
      config_(config),
      rtp_header_parser_(RtpHeaderParser::Create()) {
  DCHECK(remote_bitrate_estimator_ != nullptr);
  DCHECK(rtp_header_parser_ != nullptr);
  for (const auto& ext : config.rtp.extensions) {
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    DCHECK_GE(ext.id, 1);
    DCHECK_LE(ext.id, 14);
    if (ext.name == RtpExtension::kAudioLevel) {
      CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAudioLevel, ext.id));
    } else if (ext.name == RtpExtension::kAbsSendTime) {
      CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, ext.id));
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  return webrtc::AudioReceiveStream::Stats();
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return false;
}

bool AudioReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(packet, length, &header)) {
    return false;
  }

  // Only forward if the parsed header has absolute sender time. RTP time stamps
  // may have different rates for audio and video and shouldn't be mixed.
  if (header.extension.hasAbsoluteSendTime) {
    int64_t arrival_time_ms = TickTime::MillisecondTimestamp();
    size_t payload_size = length - header.headerLength;
    remote_bitrate_estimator_->IncomingPacket(arrival_time_ms, payload_size,
                                              header);
  }
  return true;
}
}  // namespace internal
}  // namespace webrtc
