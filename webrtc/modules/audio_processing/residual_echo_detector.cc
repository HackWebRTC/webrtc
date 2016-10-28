/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/residual_echo_detector.h"

#include "webrtc/modules/audio_processing/audio_buffer.h"

namespace webrtc {

ResidualEchoDetector::ResidualEchoDetector() = default;

ResidualEchoDetector::~ResidualEchoDetector() = default;

void ResidualEchoDetector::AnalyzeRenderAudio(
    rtc::ArrayView<const float> render_audio) const {
  // TODO(ivoc): Add implementation.
}

void ResidualEchoDetector::AnalyzeCaptureAudio(
    rtc::ArrayView<const float> render_audio) {
  // TODO(ivoc): Add implementation.
}

void ResidualEchoDetector::Initialize() {
  // TODO(ivoc): Add implementation.
}

void ResidualEchoDetector::PackRenderAudioBuffer(
    AudioBuffer* audio,
    std::vector<float>* packed_buffer) {
  RTC_DCHECK_GE(160u, audio->num_frames_per_band());

  packed_buffer->clear();
  packed_buffer->insert(packed_buffer->end(),
                        audio->split_bands_const_f(0)[kBand0To8kHz],
                        (audio->split_bands_const_f(0)[kBand0To8kHz] +
                         audio->num_frames_per_band()));
}

float ResidualEchoDetector::get_echo_likelihood() const {
  // TODO(ivoc): Add implementation.
  return 0.0f;
}

}  // namespace webrtc
