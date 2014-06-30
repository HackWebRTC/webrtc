/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * The rdft AEC algorithm, neon version of speed-critical functions.
 *
 * Based on the sse2 version.
 */


#include "webrtc/modules/audio_processing/aec/aec_rdft.h"

#include <arm_neon.h>

static const ALIGN16_BEG float ALIGN16_END
    k_swap_sign[4] = {-1.f, 1.f, -1.f, 1.f};

static void cft1st_128_neon(float* a) {
  const float32x4_t vec_swap_sign = vld1q_f32((float32_t*)k_swap_sign);
  int j, k2;

  for (k2 = 0, j = 0; j < 128; j += 16, k2 += 4) {
    float32x4_t a00v = vld1q_f32(&a[j + 0]);
    float32x4_t a04v = vld1q_f32(&a[j + 4]);
    float32x4_t a08v = vld1q_f32(&a[j + 8]);
    float32x4_t a12v = vld1q_f32(&a[j + 12]);
    float32x4_t a01v = vcombine_f32(vget_low_f32(a00v), vget_low_f32(a08v));
    float32x4_t a23v = vcombine_f32(vget_high_f32(a00v), vget_high_f32(a08v));
    float32x4_t a45v = vcombine_f32(vget_low_f32(a04v), vget_low_f32(a12v));
    float32x4_t a67v = vcombine_f32(vget_high_f32(a04v), vget_high_f32(a12v));
    const float32x4_t wk1rv = vld1q_f32(&rdft_wk1r[k2]);
    const float32x4_t wk1iv = vld1q_f32(&rdft_wk1i[k2]);
    const float32x4_t wk2rv = vld1q_f32(&rdft_wk2r[k2]);
    const float32x4_t wk2iv = vld1q_f32(&rdft_wk2i[k2]);
    const float32x4_t wk3rv = vld1q_f32(&rdft_wk3r[k2]);
    const float32x4_t wk3iv = vld1q_f32(&rdft_wk3i[k2]);
    float32x4_t x0v = vaddq_f32(a01v, a23v);
    const float32x4_t x1v = vsubq_f32(a01v, a23v);
    const float32x4_t x2v = vaddq_f32(a45v, a67v);
    const float32x4_t x3v = vsubq_f32(a45v, a67v);
    const float32x4_t x3w = vrev64q_f32(x3v);
    float32x4_t x0w;
    a01v = vaddq_f32(x0v, x2v);
    x0v = vsubq_f32(x0v, x2v);
    x0w = vrev64q_f32(x0v);
    a45v = vmulq_f32(wk2rv, x0v);
    a45v = vmlaq_f32(a45v, wk2iv, x0w);
    x0v = vmlaq_f32(x1v, x3w, vec_swap_sign);
    x0w = vrev64q_f32(x0v);
    a23v = vmulq_f32(wk1rv, x0v);
    a23v = vmlaq_f32(a23v, wk1iv, x0w);
    x0v = vmlsq_f32(x1v, x3w, vec_swap_sign);
    x0w = vrev64q_f32(x0v);
    a67v = vmulq_f32(wk3rv, x0v);
    a67v = vmlaq_f32(a67v, wk3iv, x0w);
    a00v = vcombine_f32(vget_low_f32(a01v), vget_low_f32(a23v));
    a04v = vcombine_f32(vget_low_f32(a45v), vget_low_f32(a67v));
    a08v = vcombine_f32(vget_high_f32(a01v), vget_high_f32(a23v));
    a12v = vcombine_f32(vget_high_f32(a45v), vget_high_f32(a67v));
    vst1q_f32(&a[j + 0], a00v);
    vst1q_f32(&a[j + 4], a04v);
    vst1q_f32(&a[j + 8], a08v);
    vst1q_f32(&a[j + 12], a12v);
  }
}

void aec_rdft_init_neon(void) {
  cft1st_128 = cft1st_128_neon;
}

