/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_

#include <memory>
#include <vector>

#include "webrtc/base/array_view.h"

namespace webrtc {

class AudioBuffer;
class EchoDetector;

class ResidualEchoDetector {
 public:
  ResidualEchoDetector();
  ~ResidualEchoDetector();

  // This function should be called while holding the render lock.
  void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio) const;

  // This function should be called while holding the capture lock.
  void AnalyzeCaptureAudio(rtc::ArrayView<const float> capture_audio);

  // This function should be called while holding the capture lock.
  void Initialize();

  static void PackRenderAudioBuffer(AudioBuffer* audio,
                                    std::vector<float>* packed_buffer);

  // This function should be called while holding the capture lock.
  float get_echo_likelihood() const;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_
