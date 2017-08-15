/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_PCM16B_PCM16B_COMMON_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_PCM16B_PCM16B_COMMON_H_

#include <vector>

#include "webrtc/api/audio_codecs/audio_decoder_factory.h"

namespace webrtc {
void Pcm16BAppendSupportedCodecSpecs(std::vector<AudioCodecSpec>* specs);
}

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_PCM16B_PCM16B_COMMON_H_
