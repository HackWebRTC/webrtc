/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/signal_processing/include/real_fft.h"

#include <stdlib.h>

#include "common_audio/signal_processing/include/signal_processing_library.h"

struct RealFFT {
  int order;
};

struct RealFFT* WebRtcSpl_CreateRealFFT(int order) {
  struct RealFFT* self = NULL;
  // This constraint comes from ComplexFFT().
  if (order > 10 || order < 0) {
    return NULL;
  }
  self = malloc(sizeof(struct RealFFT));
  self->order = order;
  return self;
}

void WebRtcSpl_FreeRealFFT(struct RealFFT* self) {
  free(self);
}

int WebRtcSpl_RealForwardFFT(struct RealFFT* self, int16_t* data) {
  return WebRtcSpl_ComplexFFT(data, self->order, 1);
}

int WebRtcSpl_RealInverseFFT(struct RealFFT* self, int16_t* data) {
  return WebRtcSpl_ComplexIFFT(data, self->order, 1);
}
