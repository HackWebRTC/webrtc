/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
#define WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_

#include <limits>

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

typedef std::numeric_limits<int16_t> limits_int16;

static inline int16_t RoundToInt16(float v) {
  const float kMaxRound = limits_int16::max() - 0.5f;
  const float kMinRound = limits_int16::min() + 0.5f;
  if (v > 0)
    return v >= kMaxRound ? limits_int16::max() :
                            static_cast<int16_t>(v + 0.5f);
  return v <= kMinRound ? limits_int16::min() :
                          static_cast<int16_t>(v - 0.5f);
}

// Scale (from [-1, 1]) and round to full-range int16 with clamping.
static inline int16_t ScaleAndRoundToInt16(float v) {
  if (v > 0)
    return v >= 1 ? limits_int16::max() :
                    static_cast<int16_t>(v * limits_int16::max() + 0.5f);
  return v <= -1 ? limits_int16::min() :
                   static_cast<int16_t>(-v * limits_int16::min() - 0.5f);
}

// Scale to float [-1, 1].
static inline float ScaleToFloat(int16_t v) {
  const float kMaxInt16Inverse = 1.f / limits_int16::max();
  const float kMinInt16Inverse = 1.f / limits_int16::min();
  return v * (v > 0 ? kMaxInt16Inverse : -kMinInt16Inverse);
}

// Round |size| elements of |src| to int16 with clamping and write to |dest|.
void RoundToInt16(const float* src, int size, int16_t* dest);

// Scale (from [-1, 1]) and round |size| elements of |src| to full-range int16
// with clamping and write to |dest|.
void ScaleAndRoundToInt16(const float* src, int size, int16_t* dest);

// Scale |size| elements of |src| to float [-1, 1] and write to |dest|.
void ScaleToFloat(const int16_t* src, int size, float* dest);

// Deinterleave audio from |interleaved| to the channel buffers pointed to
// by |deinterleaved|. There must be sufficient space allocated in the
// |deinterleaved| buffers (|num_channel| buffers with |samples_per_channel|
// per buffer).
template <typename T>
void Deinterleave(const T* interleaved, int samples_per_channel,
                  int num_channels, T** deinterleaved) {
  for (int i = 0; i < num_channels; ++i) {
    T* channel = deinterleaved[i];
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel; ++j) {
      channel[j] = interleaved[interleaved_idx];
      interleaved_idx += num_channels;
    }
  }
}

// Interleave audio from the channel buffers pointed to by |deinterleaved| to
// |interleaved|. There must be sufficient space allocated in |interleaved|
// (|samples_per_channel| * |num_channels|).
template <typename T>
void Interleave(const T* const* deinterleaved, int samples_per_channel,
                int num_channels, T* interleaved) {
  for (int i = 0; i < num_channels; ++i) {
    const T* channel = deinterleaved[i];
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel; ++j) {
      interleaved[interleaved_idx] = channel[j];
      interleaved_idx += num_channels;
    }
  }
}

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
