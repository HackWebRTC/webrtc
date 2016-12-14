/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/echo_canceller3.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

int EchoCanceller3::instance_count_ = 0;

EchoCanceller3::EchoCanceller3(int sample_rate_hz, bool use_anti_hum_filter) {
  int band_sample_rate_hz = (sample_rate_hz == 8000 ? sample_rate_hz : 16000);
  frame_length_ = rtc::CheckedDivExact(band_sample_rate_hz, 100);

  LOG(LS_INFO) << "AEC3 created : "
               << "{ instance_count: " << instance_count_ << "}";
  instance_count_ = rtc::AtomicOps::Increment(&instance_count_);
}

EchoCanceller3::~EchoCanceller3() = default;

bool EchoCanceller3::AnalyzeRender(AudioBuffer* render) {
  RTC_DCHECK_EQ(1u, render->num_channels());
  RTC_DCHECK_EQ(frame_length_, render->num_frames_per_band());
  return true;
}

void EchoCanceller3::AnalyzeCapture(AudioBuffer* capture) {}

void EchoCanceller3::ProcessCapture(AudioBuffer* capture,
                                    bool known_echo_path_change) {
  RTC_DCHECK_EQ(1u, capture->num_channels());
  RTC_DCHECK_EQ(frame_length_, capture->num_frames_per_band());
}

std::string EchoCanceller3::ToString(
    const AudioProcessing::Config::EchoCanceller3& config) {
  std::stringstream ss;
  ss << "{"
     << "enabled: " << (config.enabled ? "true" : "false") << "}";
  return ss.str();
}

bool EchoCanceller3::Validate(
    const AudioProcessing::Config::EchoCanceller3& config) {
  return true;
}

}  // namespace webrtc
