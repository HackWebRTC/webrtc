/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "typedefs.h"

#if defined(WEBRTC_USE_SSE2)
#include <emmintrin.h>

#include "aec_rdft.h"

static void cft1st_128_SSE2(float *a) {
  static const ALIGN16_BEG float ALIGN16_END k_swap_sign[4] =
    {-1.f, 1.f, -1.f, 1.f};
  const __m128 mm_swap_sign = _mm_load_ps(k_swap_sign);
  int j, k2;

  for (k2 = 0, j = 0; j < 128; j += 16, k2 += 4) {
          __m128 a00v   = _mm_loadu_ps(&a[j +  0]);
          __m128 a04v   = _mm_loadu_ps(&a[j +  4]);
          __m128 a08v   = _mm_loadu_ps(&a[j +  8]);
          __m128 a12v   = _mm_loadu_ps(&a[j + 12]);
          __m128 a01v   = _mm_shuffle_ps(a00v, a08v, _MM_SHUFFLE(1, 0, 1 ,0));
          __m128 a23v   = _mm_shuffle_ps(a00v, a08v, _MM_SHUFFLE(3, 2, 3 ,2));
          __m128 a45v   = _mm_shuffle_ps(a04v, a12v, _MM_SHUFFLE(1, 0, 1 ,0));
          __m128 a67v   = _mm_shuffle_ps(a04v, a12v, _MM_SHUFFLE(3, 2, 3 ,2));

    const __m128 wk1rv  = _mm_load_ps(&rdft_wk1r[k2]);
    const __m128 wk1iv  = _mm_load_ps(&rdft_wk1i[k2]);
    const __m128 wk2rv  = _mm_load_ps(&rdft_wk2r[k2]);
    const __m128 wk2iv  = _mm_load_ps(&rdft_wk2i[k2]);
    const __m128 wk3rv  = _mm_load_ps(&rdft_wk3r[k2]);
    const __m128 wk3iv  = _mm_load_ps(&rdft_wk3i[k2]);
          __m128 x0v    = _mm_add_ps(a01v, a23v);
    const __m128 x1v    = _mm_sub_ps(a01v, a23v);
    const __m128 x2v    = _mm_add_ps(a45v, a67v);
    const __m128 x3v    = _mm_sub_ps(a45v, a67v);
                 a01v   = _mm_add_ps(x0v, x2v);
                 x0v    = _mm_sub_ps(x0v, x2v);
          __m128 x0w    = _mm_shuffle_ps(x0v, x0v, _MM_SHUFFLE(2, 3, 0 ,1));

    const __m128 a45_0v = _mm_mul_ps(wk2rv, x0v);
    const __m128 a45_1v = _mm_mul_ps(wk2iv, x0w);
                 a45v   = _mm_add_ps(a45_0v, a45_1v);

    const __m128 x3w    = _mm_shuffle_ps(x3v, x3v, _MM_SHUFFLE(2, 3, 0 ,1));
    const __m128 x3s    = _mm_mul_ps(mm_swap_sign, x3w);
                 x0v    = _mm_add_ps(x1v, x3s);
                 x0w    = _mm_shuffle_ps(x0v, x0v, _MM_SHUFFLE(2, 3, 0 ,1));
    const __m128 a23_0v = _mm_mul_ps(wk1rv, x0v);
    const __m128 a23_1v = _mm_mul_ps(wk1iv, x0w);
                 a23v   = _mm_add_ps(a23_0v, a23_1v);

                 x0v    = _mm_sub_ps(x1v, x3s);
                 x0w    = _mm_shuffle_ps(x0v, x0v, _MM_SHUFFLE(2, 3, 0 ,1));
    const __m128 a67_0v = _mm_mul_ps(wk3rv, x0v);
    const __m128 a67_1v = _mm_mul_ps(wk3iv, x0w);
                 a67v   = _mm_add_ps(a67_0v, a67_1v);

                 a00v   = _mm_shuffle_ps(a01v, a23v, _MM_SHUFFLE(1, 0, 1 ,0));
                 a04v   = _mm_shuffle_ps(a45v, a67v, _MM_SHUFFLE(1, 0, 1 ,0));
                 a08v   = _mm_shuffle_ps(a01v, a23v, _MM_SHUFFLE(3, 2, 3 ,2));
                 a12v   = _mm_shuffle_ps(a45v, a67v, _MM_SHUFFLE(3, 2, 3 ,2));
    _mm_storeu_ps(&a[j +  0], a00v);
    _mm_storeu_ps(&a[j +  4], a04v);
    _mm_storeu_ps(&a[j +  8], a08v);
    _mm_storeu_ps(&a[j + 12], a12v);
  }
}

static void rftfsub_128_SSE2(float *a) {
  const float *c = rdft_w + 32;
  int j1, j2, k1, k2;
  float wkr, wki, xr, xi, yr, yi;

  static const ALIGN16_BEG float ALIGN16_END k_half[4] =
    {0.5f, 0.5f, 0.5f, 0.5f};
  const __m128 mm_half = _mm_load_ps(k_half);

  // Vectorized code (four at once).
  //    Note: commented number are indexes for the first iteration of the loop.
  for (j1 = 1, j2 = 2; j2 + 7 < 64; j1 += 4, j2 += 8) {
    // Load 'wk'.
    const __m128 c_j1 = _mm_loadu_ps(&c[     j1]);         //  1,  2,  3,  4,
    const __m128 c_k1 = _mm_loadu_ps(&c[29 - j1]);         // 28, 29, 30, 31,
    const __m128 wkrt = _mm_sub_ps(mm_half, c_k1);         // 28, 29, 30, 31,
    const __m128 wkr_ =
      _mm_shuffle_ps(wkrt, wkrt, _MM_SHUFFLE(0, 1, 2, 3)); // 31, 30, 29, 28,
    const __m128 wki_ = c_j1;                              //  1,  2,  3,  4,
    // Load and shuffle 'a'.
    const __m128 a_j2_0 = _mm_loadu_ps(&a[0   + j2]);  //   2,   3,   4,   5,
    const __m128 a_j2_4 = _mm_loadu_ps(&a[4   + j2]);  //   6,   7,   8,   9,
    const __m128 a_k2_0 = _mm_loadu_ps(&a[122 - j2]);  // 120, 121, 122, 123,
    const __m128 a_k2_4 = _mm_loadu_ps(&a[126 - j2]);  // 124, 125, 126, 127,
    const __m128 a_j2_p0 = _mm_shuffle_ps(a_j2_0, a_j2_4,
                            _MM_SHUFFLE(2, 0, 2 ,0));  //   2,   4,   6,   8,
    const __m128 a_j2_p1 = _mm_shuffle_ps(a_j2_0, a_j2_4,
                            _MM_SHUFFLE(3, 1, 3 ,1));  //   3,   5,   7,   9,
    const __m128 a_k2_p0 = _mm_shuffle_ps(a_k2_4, a_k2_0,
                            _MM_SHUFFLE(0, 2, 0 ,2));  // 126, 124, 122, 120,
    const __m128 a_k2_p1 = _mm_shuffle_ps(a_k2_4, a_k2_0,
                            _MM_SHUFFLE(1, 3, 1 ,3));  // 127, 125, 123, 121,
    // Calculate 'x'.
    const __m128 xr_ = _mm_sub_ps(a_j2_p0, a_k2_p0);
                                               // 2-126, 4-124, 6-122, 8-120,
    const __m128 xi_ = _mm_add_ps(a_j2_p1, a_k2_p1);
                                               // 3-127, 5-125, 7-123, 9-121,
    // Calculate product into 'y'.
    //    yr = wkr * xr - wki * xi;
    //    yi = wkr * xi + wki * xr;
    const __m128 a_ = _mm_mul_ps(wkr_, xr_);
    const __m128 b_ = _mm_mul_ps(wki_, xi_);
    const __m128 c_ = _mm_mul_ps(wkr_, xi_);
    const __m128 d_ = _mm_mul_ps(wki_, xr_);
    const __m128 yr_ = _mm_sub_ps(a_, b_);     // 2-126, 4-124, 6-122, 8-120,
    const __m128 yi_ = _mm_add_ps(c_, d_);     // 3-127, 5-125, 7-123, 9-121,
    // Update 'a'.
    //    a[j2 + 0] -= yr;
    //    a[j2 + 1] -= yi;
    //    a[k2 + 0] += yr;
    //    a[k2 + 1] -= yi;
    const __m128 a_j2_p0n = _mm_sub_ps(a_j2_p0, yr_);  //   2,   4,   6,   8,
    const __m128 a_j2_p1n = _mm_sub_ps(a_j2_p1, yi_);  //   3,   5,   7,   9,
    const __m128 a_k2_p0n = _mm_add_ps(a_k2_p0, yr_);  // 126, 124, 122, 120,
    const __m128 a_k2_p1n = _mm_sub_ps(a_k2_p1, yi_);  // 127, 125, 123, 121,
    // Shuffle in right order and store.
    const __m128 a_j2_0n = _mm_unpacklo_ps(a_j2_p0n, a_j2_p1n);
                                                       //   2,   3,   4,   5,
    const __m128 a_j2_4n = _mm_unpackhi_ps(a_j2_p0n, a_j2_p1n);
                                                       //   6,   7,   8,   9,
    const __m128 a_k2_0nt = _mm_unpackhi_ps(a_k2_p0n, a_k2_p1n);
                                                       // 122, 123, 120, 121,
    const __m128 a_k2_4nt = _mm_unpacklo_ps(a_k2_p0n, a_k2_p1n);
                                                       // 126, 127, 124, 125,
    const __m128 a_k2_0n = _mm_shuffle_ps(a_k2_0nt, a_k2_0nt,
                            _MM_SHUFFLE(1, 0, 3 ,2));  // 120, 121, 122, 123,
    const __m128 a_k2_4n = _mm_shuffle_ps(a_k2_4nt, a_k2_4nt,
                            _MM_SHUFFLE(1, 0, 3 ,2));  // 124, 125, 126, 127,
    _mm_storeu_ps(&a[0   + j2], a_j2_0n);
    _mm_storeu_ps(&a[4   + j2], a_j2_4n);
    _mm_storeu_ps(&a[122 - j2], a_k2_0n);
    _mm_storeu_ps(&a[126 - j2], a_k2_4n);
  }
  // Scalar code for the remaining items.
  for (; j2 < 64; j1 += 1, j2 += 2) {
    k2 = 128 - j2;
    k1 =  32 - j1;
    wkr = 0.5f - c[k1];
    wki = c[j1];
    xr = a[j2 + 0] - a[k2 + 0];
    xi = a[j2 + 1] + a[k2 + 1];
    yr = wkr * xr - wki * xi;
    yi = wkr * xi + wki * xr;
    a[j2 + 0] -= yr;
    a[j2 + 1] -= yi;
    a[k2 + 0] += yr;
    a[k2 + 1] -= yi;
  }
}

static void rftbsub_128_SSE2(float *a) {
  const float *c = rdft_w + 32;
  int j1, j2, k1, k2;
  float wkr, wki, xr, xi, yr, yi;

  static const ALIGN16_BEG float ALIGN16_END k_half[4] =
    {0.5f, 0.5f, 0.5f, 0.5f};
  const __m128 mm_half = _mm_load_ps(k_half);

  a[1] = -a[1];
  // Vectorized code (four at once).
  //    Note: commented number are indexes for the first iteration of the loop.
  for (j1 = 1, j2 = 2; j2 + 7 < 64; j1 += 4, j2 += 8) {
    // Load 'wk'.
    const __m128 c_j1 = _mm_loadu_ps(&c[     j1]);         //  1,  2,  3,  4,
    const __m128 c_k1 = _mm_loadu_ps(&c[29 - j1]);         // 28, 29, 30, 31,
    const __m128 wkrt = _mm_sub_ps(mm_half, c_k1);         // 28, 29, 30, 31,
    const __m128 wkr_ =
      _mm_shuffle_ps(wkrt, wkrt, _MM_SHUFFLE(0, 1, 2, 3)); // 31, 30, 29, 28,
    const __m128 wki_ = c_j1;                              //  1,  2,  3,  4,
    // Load and shuffle 'a'.
    const __m128 a_j2_0 = _mm_loadu_ps(&a[0   + j2]);  //   2,   3,   4,   5,
    const __m128 a_j2_4 = _mm_loadu_ps(&a[4   + j2]);  //   6,   7,   8,   9,
    const __m128 a_k2_0 = _mm_loadu_ps(&a[122 - j2]);  // 120, 121, 122, 123,
    const __m128 a_k2_4 = _mm_loadu_ps(&a[126 - j2]);  // 124, 125, 126, 127,
    const __m128 a_j2_p0 = _mm_shuffle_ps(a_j2_0, a_j2_4,
                            _MM_SHUFFLE(2, 0, 2 ,0));  //   2,   4,   6,   8,
    const __m128 a_j2_p1 = _mm_shuffle_ps(a_j2_0, a_j2_4,
                            _MM_SHUFFLE(3, 1, 3 ,1));  //   3,   5,   7,   9,
    const __m128 a_k2_p0 = _mm_shuffle_ps(a_k2_4, a_k2_0,
                            _MM_SHUFFLE(0, 2, 0 ,2));  // 126, 124, 122, 120,
    const __m128 a_k2_p1 = _mm_shuffle_ps(a_k2_4, a_k2_0,
                            _MM_SHUFFLE(1, 3, 1 ,3));  // 127, 125, 123, 121,
    // Calculate 'x'.
    const __m128 xr_ = _mm_sub_ps(a_j2_p0, a_k2_p0);
                                               // 2-126, 4-124, 6-122, 8-120,
    const __m128 xi_ = _mm_add_ps(a_j2_p1, a_k2_p1);
                                               // 3-127, 5-125, 7-123, 9-121,
    // Calculate product into 'y'.
    //    yr = wkr * xr + wki * xi;
    //    yi = wkr * xi - wki * xr;
    const __m128 a_ = _mm_mul_ps(wkr_, xr_);
    const __m128 b_ = _mm_mul_ps(wki_, xi_);
    const __m128 c_ = _mm_mul_ps(wkr_, xi_);
    const __m128 d_ = _mm_mul_ps(wki_, xr_);
    const __m128 yr_ = _mm_add_ps(a_, b_);     // 2-126, 4-124, 6-122, 8-120,
    const __m128 yi_ = _mm_sub_ps(c_, d_);     // 3-127, 5-125, 7-123, 9-121,
    // Update 'a'.
    //    a[j2 + 0] = a[j2 + 0] - yr;
    //    a[j2 + 1] = yi - a[j2 + 1];
    //    a[k2 + 0] = yr + a[k2 + 0];
    //    a[k2 + 1] = yi - a[k2 + 1];
    const __m128 a_j2_p0n = _mm_sub_ps(a_j2_p0, yr_);  //   2,   4,   6,   8,
    const __m128 a_j2_p1n = _mm_sub_ps(yi_, a_j2_p1);  //   3,   5,   7,   9,
    const __m128 a_k2_p0n = _mm_add_ps(a_k2_p0, yr_);  // 126, 124, 122, 120,
    const __m128 a_k2_p1n = _mm_sub_ps(yi_, a_k2_p1);  // 127, 125, 123, 121,
    // Shuffle in right order and store.
    // Shuffle in right order and store.
    const __m128 a_j2_0n = _mm_unpacklo_ps(a_j2_p0n, a_j2_p1n);
                                                       //   2,   3,   4,   5,
    const __m128 a_j2_4n = _mm_unpackhi_ps(a_j2_p0n, a_j2_p1n);
                                                       //   6,   7,   8,   9,
    const __m128 a_k2_0nt = _mm_unpackhi_ps(a_k2_p0n, a_k2_p1n);
                                                       // 122, 123, 120, 121,
    const __m128 a_k2_4nt = _mm_unpacklo_ps(a_k2_p0n, a_k2_p1n);
                                                       // 126, 127, 124, 125,
    const __m128 a_k2_0n = _mm_shuffle_ps(a_k2_0nt, a_k2_0nt,
                            _MM_SHUFFLE(1, 0, 3 ,2));  // 120, 121, 122, 123,
    const __m128 a_k2_4n = _mm_shuffle_ps(a_k2_4nt, a_k2_4nt,
                            _MM_SHUFFLE(1, 0, 3 ,2));  // 124, 125, 126, 127,
    _mm_storeu_ps(&a[0   + j2], a_j2_0n);
    _mm_storeu_ps(&a[4   + j2], a_j2_4n);
    _mm_storeu_ps(&a[122 - j2], a_k2_0n);
    _mm_storeu_ps(&a[126 - j2], a_k2_4n);
  }
  // Scalar code for the remaining items.
  for (; j2 < 64; j1 += 1, j2 += 2) {
    k2 = 128 - j2;
    k1 =  32 - j1;
    wkr = 0.5f - c[k1];
    wki = c[j1];
    xr = a[j2 + 0] - a[k2 + 0];
    xi = a[j2 + 1] + a[k2 + 1];
    yr = wkr * xr + wki * xi;
    yi = wkr * xi - wki * xr;
    a[j2 + 0] = a[j2 + 0] - yr;
    a[j2 + 1] = yi - a[j2 + 1];
    a[k2 + 0] = yr + a[k2 + 0];
    a[k2 + 1] = yi - a[k2 + 1];
  }
  a[65] = -a[65];
}

void aec_rdft_init_sse2(void) {
  cft1st_128 = cft1st_128_SSE2;
  rftfsub_128 = rftfsub_128_SSE2;
  rftbsub_128 = rftbsub_128_SSE2;
}

#endif  // WEBRTC_USE_SS2
