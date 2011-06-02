/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * The core AEC algorithm, SSE2 version of speed-critical functions.
 */

#if defined(__SSE2__)
#include <emmintrin.h>
#include <math.h>

#include "aec_core.h"

__inline static float MulRe(float aRe, float aIm, float bRe, float bIm)
{
  return aRe * bRe - aIm * bIm;
}

__inline static float MulIm(float aRe, float aIm, float bRe, float bIm)
{
  return aRe * bIm + aIm * bRe;
}

static void FilterFarSSE2(aec_t *aec, float yf[2][PART_LEN1])
{
  int i;
  for (i = 0; i < NR_PART; i++) {
    int j;
    int xPos = (i + aec->xfBufBlockPos) * PART_LEN1;
    int pos = i * PART_LEN1;
    // Check for wrap
    if (i + aec->xfBufBlockPos >= NR_PART) {
      xPos -= NR_PART*(PART_LEN1);
    }

    // vectorized code (four at once)
    for (j = 0; j + 3 < PART_LEN1; j += 4) {
      const __m128 xfBuf_re = _mm_loadu_ps(&aec->xfBuf[0][xPos + j]);
      const __m128 xfBuf_im = _mm_loadu_ps(&aec->xfBuf[1][xPos + j]);
      const __m128 wfBuf_re = _mm_loadu_ps(&aec->wfBuf[0][pos + j]);
      const __m128 wfBuf_im = _mm_loadu_ps(&aec->wfBuf[1][pos + j]);
      const __m128 yf_re = _mm_loadu_ps(&yf[0][j]);
      const __m128 yf_im = _mm_loadu_ps(&yf[1][j]);
      const __m128 a = _mm_mul_ps(xfBuf_re, wfBuf_re);
      const __m128 b = _mm_mul_ps(xfBuf_im, wfBuf_im);
      const __m128 c = _mm_mul_ps(xfBuf_re, wfBuf_im);
      const __m128 d = _mm_mul_ps(xfBuf_im, wfBuf_re);
      const __m128 e = _mm_sub_ps(a, b);
      const __m128 f = _mm_add_ps(c, d);
      const __m128 g = _mm_add_ps(yf_re, e);
      const __m128 h = _mm_add_ps(yf_im, f);
      _mm_storeu_ps(&yf[0][j], g);
      _mm_storeu_ps(&yf[1][j], h);
    }
    // scalar code for the remaining items.
    for (; j < PART_LEN1; j++) {
      yf[0][j] += MulRe(aec->xfBuf[0][xPos + j], aec->xfBuf[1][xPos + j],
                        aec->wfBuf[0][ pos + j], aec->wfBuf[1][ pos + j]);
      yf[1][j] += MulIm(aec->xfBuf[0][xPos + j], aec->xfBuf[1][xPos + j],
                        aec->wfBuf[0][ pos + j], aec->wfBuf[1][ pos + j]);
    }
  }
}

static void ScaleErrorSignalSSE2(aec_t *aec, float ef[2][PART_LEN1])
{
  const __m128 k1e_10f = _mm_set1_ps(1e-10f);
  const __m128 kThresh = _mm_set1_ps(aec->errThresh);
  const __m128 kMu = _mm_set1_ps(aec->mu);

  int i;
  // vectorized code (four at once)
  for (i = 0; i + 3 < PART_LEN1; i += 4) {
    const __m128 xPow = _mm_loadu_ps(&aec->xPow[i]);
    const __m128 ef_re_base = _mm_loadu_ps(&ef[0][i]);
    const __m128 ef_im_base = _mm_loadu_ps(&ef[1][i]);

    const __m128 xPowPlus = _mm_add_ps(xPow, k1e_10f);
    __m128 ef_re = _mm_div_ps(ef_re_base, xPowPlus);
    __m128 ef_im = _mm_div_ps(ef_im_base, xPowPlus);
    const __m128 ef_re2 = _mm_mul_ps(ef_re, ef_re);
    const __m128 ef_im2 = _mm_mul_ps(ef_im, ef_im);
    const __m128 ef_sum2 = _mm_add_ps(ef_re2, ef_im2);
    const __m128 absEf = _mm_sqrt_ps(ef_sum2);
    const __m128 bigger = _mm_cmpgt_ps(absEf, kThresh);
    __m128 absEfPlus = _mm_add_ps(absEf, k1e_10f);
    const __m128 absEfInv = _mm_div_ps(kThresh, absEfPlus);
    __m128 ef_re_if = _mm_mul_ps(ef_re, absEfInv);
    __m128 ef_im_if = _mm_mul_ps(ef_im, absEfInv);
    ef_re_if = _mm_and_ps(bigger, ef_re_if);
    ef_im_if = _mm_and_ps(bigger, ef_im_if);
    ef_re = _mm_andnot_ps(bigger, ef_re);
    ef_im = _mm_andnot_ps(bigger, ef_im);
    ef_re = _mm_or_ps(ef_re, ef_re_if);
    ef_im = _mm_or_ps(ef_im, ef_im_if);
    ef_re = _mm_mul_ps(ef_re, kMu);
    ef_im = _mm_mul_ps(ef_im, kMu);

    _mm_storeu_ps(&ef[0][i], ef_re);
    _mm_storeu_ps(&ef[1][i], ef_im);
  }
  // scalar code for the remaining items.
  for (; i < (PART_LEN1); i++) {
    float absEf;
    ef[0][i] /= (aec->xPow[i] + 1e-10f);
    ef[1][i] /= (aec->xPow[i] + 1e-10f);
    absEf = sqrtf(ef[0][i] * ef[0][i] + ef[1][i] * ef[1][i]);

    if (absEf > aec->errThresh) {
      absEf = aec->errThresh / (absEf + 1e-10f);
      ef[0][i] *= absEf;
      ef[1][i] *= absEf;
    }

    // Stepsize factor
    ef[0][i] *= aec->mu;
    ef[1][i] *= aec->mu;
  }
}

static void FilterAdaptationSSE2(aec_t *aec, float *fft, float ef[2][PART_LEN1],
                          int ip[IP_LEN], float wfft[W_LEN]) {
  int i, j;
  for (i = 0; i < NR_PART; i++) {
    int xPos = (i + aec->xfBufBlockPos)*(PART_LEN1);
    int pos = i * PART_LEN1;
    // Check for wrap
    if (i + aec->xfBufBlockPos >= NR_PART) {
      xPos -= NR_PART * PART_LEN1;
    }

#ifdef UNCONSTR
    for (j = 0; j < PART_LEN1; j++) {
      aec->wfBuf[pos + j][0] += MulRe(aec->xfBuf[xPos + j][0],
                                      -aec->xfBuf[xPos + j][1],
                                      ef[j][0], ef[j][1]);
      aec->wfBuf[pos + j][1] += MulIm(aec->xfBuf[xPos + j][0],
                                      -aec->xfBuf[xPos + j][1],
                                      ef[j][0], ef[j][1]);
    }
#else
    // Process the whole array...
    for (j = 0; j < PART_LEN; j+= 4) {
      // Load xfBuf and ef.
      const __m128 xfBuf_re = _mm_loadu_ps(&aec->xfBuf[0][xPos + j]);
      const __m128 xfBuf_im = _mm_loadu_ps(&aec->xfBuf[1][xPos + j]);
      const __m128 ef_re = _mm_loadu_ps(&ef[0][j]);
      const __m128 ef_im = _mm_loadu_ps(&ef[1][j]);
      // Calculate the product of conjugate(xfBuf) by ef.
      //   re(conjugate(a) * b) = aRe * bRe + aIm * bIm
      //   im(conjugate(a) * b)=  aRe * bIm - aIm * bRe
      const __m128 a = _mm_mul_ps(xfBuf_re, ef_re);
      const __m128 b = _mm_mul_ps(xfBuf_im, ef_im);
      const __m128 c = _mm_mul_ps(xfBuf_re, ef_im);
      const __m128 d = _mm_mul_ps(xfBuf_im, ef_re);
      const __m128 e = _mm_add_ps(a, b);
      const __m128 f = _mm_sub_ps(c, d);
      // Interleave real and imaginary parts.
      const __m128 g = _mm_unpacklo_ps(e, f);
      const __m128 h = _mm_unpackhi_ps(e, f);
      // Store
      _mm_storeu_ps(&fft[2*j + 0], g);
      _mm_storeu_ps(&fft[2*j + 4], h);
    }
    // ... and fixup the first imaginary entry.
    fft[1] = MulRe(aec->xfBuf[0][xPos + PART_LEN],
                   -aec->xfBuf[1][xPos + PART_LEN],
                   ef[0][PART_LEN], ef[1][PART_LEN]);

    rdft(PART_LEN2, -1, fft, ip, wfft);
    memset(fft + PART_LEN, 0, sizeof(float)*PART_LEN);

    // fft scaling
    {
      float scale = 2.0f / PART_LEN2;
      const __m128 scale_ps = _mm_load_ps1(&scale);
      for (j = 0; j < PART_LEN; j+=4) {
        const __m128 fft_ps = _mm_loadu_ps(&fft[j]);
        const __m128 fft_scale = _mm_mul_ps(fft_ps, scale_ps);
        _mm_storeu_ps(&fft[j], fft_scale);
      }
    }
    rdft(PART_LEN2, 1, fft, ip, wfft);

    {
      float wt1 = aec->wfBuf[1][pos];
      aec->wfBuf[0][pos + PART_LEN] += fft[1];
      for (j = 0; j < PART_LEN; j+= 4) {
        __m128 wtBuf_re = _mm_loadu_ps(&aec->wfBuf[0][pos + j]);
        __m128 wtBuf_im = _mm_loadu_ps(&aec->wfBuf[1][pos + j]);
        const __m128 fft0 = _mm_loadu_ps(&fft[2 * j + 0]);
        const __m128 fft4 = _mm_loadu_ps(&fft[2 * j + 4]);
        const __m128 fft_re = _mm_shuffle_ps(fft0, fft4, _MM_SHUFFLE(2, 0, 2 ,0));
        const __m128 fft_im = _mm_shuffle_ps(fft0, fft4, _MM_SHUFFLE(3, 1, 3 ,1));
        wtBuf_re = _mm_add_ps(wtBuf_re, fft_re);
        wtBuf_im = _mm_add_ps(wtBuf_im, fft_im);
        _mm_storeu_ps(&aec->wfBuf[0][pos + j], wtBuf_re);
        _mm_storeu_ps(&aec->wfBuf[1][pos + j], wtBuf_im);
      }
      aec->wfBuf[1][pos] = wt1;
    }
#endif // UNCONSTR
  }
}

void WebRtcAec_InitAec_SSE2(void) {
  WebRtcAec_FilterFar = FilterFarSSE2;
  WebRtcAec_ScaleErrorSignal = ScaleErrorSignalSSE2;
  WebRtcAec_FilterAdaptation = FilterAdaptationSSE2;
}

#endif   //__SSE2__
