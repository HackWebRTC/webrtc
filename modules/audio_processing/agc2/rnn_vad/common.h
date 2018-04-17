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
constexpr size_t kFrameSize10ms24kHz = 240;

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_COMMON_H_
