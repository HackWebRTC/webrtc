/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/include/audio_util.h"

#include "webrtc/typedefs.h"

namespace webrtc {

void RoundToInt16(const float* src, int size, int16_t* dest) {
  for (int i = 0; i < size; ++i)
    dest[i] = RoundToInt16(src[i]);
}

void ScaleAndRoundToInt16(const float* src, int size, int16_t* dest) {
  for (int i = 0; i < size; ++i)
    dest[i] = ScaleAndRoundToInt16(src[i]);
}

void ScaleToFloat(const int16_t* src, int size, float* dest) {
  for (int i = 0; i < size; ++i)
    dest[i] = ScaleToFloat(src[i]);
}

}  // namespace webrtc
