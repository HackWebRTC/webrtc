/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_CANCELLER3_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_CANCELLER3_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"

namespace webrtc {

class EchoCanceller3 {
 public:
  EchoCanceller3(int sample_rate_hz, bool use_anti_hum_filter);
  ~EchoCanceller3();
  // Analyzes and stores an internal copy of the split-band domain render
  // signal.
  bool AnalyzeRender(AudioBuffer* farend);
  // Analyzes the full-band domain capture signal to detect signal saturation.
  void AnalyzeCapture(AudioBuffer* capture);
  // Processes the split-band domain capture signal in order to remove any echo
  // present in the signal.
  void ProcessCapture(AudioBuffer* capture, bool known_echo_path_change);

  // Validates a config.
  static bool Validate(const AudioProcessing::Config::EchoCanceller3& config);
  // Dumps a config to a string.
  static std::string ToString(
      const AudioProcessing::Config::EchoCanceller3& config);

 private:
  static int instance_count_;
  size_t frame_length_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoCanceller3);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_CANCELLER3_H_
