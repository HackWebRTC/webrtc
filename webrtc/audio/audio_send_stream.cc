/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_send_stream.h"

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {
std::string AudioSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrc: " << ssrc;
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

std::string AudioSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", voe_channel_id: " << voe_channel_id;
  // TODO(solenberg): Encoder config.
  ss << ", cng_payload_type: " << cng_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << '}';
  return ss.str();
}

namespace internal {
AudioSendStream::AudioSendStream(const webrtc::AudioSendStream::Config& config)
    : config_(config) {
  LOG(LS_INFO) << "AudioSendStream: " << config_.ToString();
  RTC_DCHECK(config.voe_channel_id != -1);
}

AudioSendStream::~AudioSendStream() {
  LOG(LS_INFO) << "~AudioSendStream: " << config_.ToString();
}

webrtc::AudioSendStream::Stats AudioSendStream::GetStats() const {
  return webrtc::AudioSendStream::Stats();
}

void AudioSendStream::Start() {
}

void AudioSendStream::Stop() {
}

void AudioSendStream::SignalNetworkState(NetworkState state) {
}

bool AudioSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return false;
}
}  // namespace internal
}  // namespace webrtc
