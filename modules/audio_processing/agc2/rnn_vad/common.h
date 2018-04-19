/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_

namespace webrtc {
namespace rnn_vad {

constexpr size_t kSampleRate24kHz = 24000;
constexpr size_t kFrameSize10ms24kHz = kSampleRate24kHz / 100;
constexpr size_t kFrameSize20ms24kHz = kFrameSize10ms24kHz * 2;

// Pitch analysis params.
constexpr size_t kPitchMinPeriod24kHz = kSampleRate24kHz / 800;   // 0.00125 s.
constexpr size_t kPitchMaxPeriod24kHz = kSampleRate24kHz / 62.5;  // 0.016 s.
constexpr size_t kBufSize24kHz = kPitchMaxPeriod24kHz + kFrameSize20ms24kHz;
static_assert((kBufSize24kHz & 1) == 0, "The buffer size must be even.");

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_
