/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_
#define WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_

#include "typedefs.h"

struct RealFFT;

#ifdef __cplusplus
extern "C" {
#endif

// TODO(andrew): documentation.
struct RealFFT* WebRtcSpl_CreateRealFFT(int order);
void WebRtcSpl_FreeRealFFT(struct RealFFT* self);

// TODO(andrew): This currently functions exactly the same as ComplexFFT().
// Manage the surrounding operations (ComplexBitReverse etc) here instead.
//
// data must be of length 2^(order + 1) to hold the complex output.
int WebRtcSpl_RealForwardFFT(struct RealFFT* self, int16_t* data);
int WebRtcSpl_RealInverseFFT(struct RealFFT* self, int16_t* data);

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_
