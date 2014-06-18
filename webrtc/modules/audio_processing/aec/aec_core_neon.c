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
 * The core AEC algorithm, neon version of speed-critical functions.
 *
 * Based on aec_core_sse2.c.
 */

#include "webrtc/modules/audio_processing/aec/aec_core.h"

#include <arm_neon.h>
#include <math.h>
#include <string.h>  // memset

#include "webrtc/modules/audio_processing/aec/aec_core_internal.h"
#include "webrtc/modules/audio_processing/aec/aec_rdft.h"

enum { kShiftExponentIntoTopMantissa = 8 };
enum { kFloatExponentShift = 23 };

__inline static float MulRe(float aRe, float aIm, float bRe, float bIm) {
  return aRe * bRe - aIm * bIm;
}

static void FilterAdaptationNEON(AecCore* aec,
                                 float* fft,
                                 float ef[2][PART_LEN1]) {
  int i;
  const int num_partitions = aec->num_partitions;
  for (i = 0; i < num_partitions; i++) {
    int xPos = (i + aec->xfBufBlockPos) * PART_LEN1;
    int pos = i * PART_LEN1;
    int j;
    // Check for wrap
    if (i + aec->xfBufBlockPos >= num_partitions) {
      xPos -= num_partitions * PART_LEN1;
    }

    // Process the whole array...
    for (j = 0; j < PART_LEN; j += 4) {
      // Load xfBuf and ef.
      const float32x4_t xfBuf_re = vld1q_f32(&aec->xfBuf[0][xPos + j]);
      const float32x4_t xfBuf_im = vld1q_f32(&aec->xfBuf[1][xPos + j]);
      const float32x4_t ef_re = vld1q_f32(&ef[0][j]);
      const float32x4_t ef_im = vld1q_f32(&ef[1][j]);
      // Calculate the product of conjugate(xfBuf) by ef.
      //   re(conjugate(a) * b) = aRe * bRe + aIm * bIm
      //   im(conjugate(a) * b)=  aRe * bIm - aIm * bRe
      const float32x4_t a = vmulq_f32(xfBuf_re, ef_re);
      const float32x4_t e = vmlaq_f32(a, xfBuf_im, ef_im);
      const float32x4_t c = vmulq_f32(xfBuf_re, ef_im);
      const float32x4_t f = vmlsq_f32(c, xfBuf_im, ef_re);
      // Interleave real and imaginary parts.
      const float32x4x2_t g_n_h = vzipq_f32(e, f);
      // Store
      vst1q_f32(&fft[2 * j + 0], g_n_h.val[0]);
      vst1q_f32(&fft[2 * j + 4], g_n_h.val[1]);
    }
    // ... and fixup the first imaginary entry.
    fft[1] = MulRe(aec->xfBuf[0][xPos + PART_LEN],
                   -aec->xfBuf[1][xPos + PART_LEN],
                   ef[0][PART_LEN],
                   ef[1][PART_LEN]);

    aec_rdft_inverse_128(fft);
    memset(fft + PART_LEN, 0, sizeof(float) * PART_LEN);

    // fft scaling
    {
      const float scale = 2.0f / PART_LEN2;
      const float32x4_t scale_ps = vmovq_n_f32(scale);
      for (j = 0; j < PART_LEN; j += 4) {
        const float32x4_t fft_ps = vld1q_f32(&fft[j]);
        const float32x4_t fft_scale = vmulq_f32(fft_ps, scale_ps);
        vst1q_f32(&fft[j], fft_scale);
      }
    }
    aec_rdft_forward_128(fft);

    {
      const float wt1 = aec->wfBuf[1][pos];
      aec->wfBuf[0][pos + PART_LEN] += fft[1];
      for (j = 0; j < PART_LEN; j += 4) {
        float32x4_t wtBuf_re = vld1q_f32(&aec->wfBuf[0][pos + j]);
        float32x4_t wtBuf_im = vld1q_f32(&aec->wfBuf[1][pos + j]);
        const float32x4_t fft0 = vld1q_f32(&fft[2 * j + 0]);
        const float32x4_t fft4 = vld1q_f32(&fft[2 * j + 4]);
        const float32x4x2_t fft_re_im = vuzpq_f32(fft0, fft4);
        wtBuf_re = vaddq_f32(wtBuf_re, fft_re_im.val[0]);
        wtBuf_im = vaddq_f32(wtBuf_im, fft_re_im.val[1]);

        vst1q_f32(&aec->wfBuf[0][pos + j], wtBuf_re);
        vst1q_f32(&aec->wfBuf[1][pos + j], wtBuf_im);
      }
      aec->wfBuf[1][pos] = wt1;
    }
  }
}

extern const float WebRtcAec_weightCurve[65];
extern const float WebRtcAec_overDriveCurve[65];

static float32x4_t vpowq_f32(float32x4_t a, float32x4_t b) {
  // a^b = exp2(b * log2(a))
  //   exp2(x) and log2(x) are calculated using polynomial approximations.
  float32x4_t log2_a, b_log2_a, a_exp_b;

  // Calculate log2(x), x = a.
  {
    // To calculate log2(x), we decompose x like this:
    //   x = y * 2^n
    //     n is an integer
    //     y is in the [1.0, 2.0) range
    //
    //   log2(x) = log2(y) + n
    //     n       can be evaluated by playing with float representation.
    //     log2(y) in a small range can be approximated, this code uses an order
    //             five polynomial approximation. The coefficients have been
    //             estimated with the Remez algorithm and the resulting
    //             polynomial has a maximum relative error of 0.00086%.

    // Compute n.
    //    This is done by masking the exponent, shifting it into the top bit of
    //    the mantissa, putting eight into the biased exponent (to shift/
    //    compensate the fact that the exponent has been shifted in the top/
    //    fractional part and finally getting rid of the implicit leading one
    //    from the mantissa by substracting it out.
    const uint32x4_t vec_float_exponent_mask = vdupq_n_u32(0x7F800000);
    const uint32x4_t vec_eight_biased_exponent = vdupq_n_u32(0x43800000);
    const uint32x4_t vec_implicit_leading_one = vdupq_n_u32(0x43BF8000);
    const uint32x4_t two_n = vandq_u32(vreinterpretq_u32_f32(a),
                                       vec_float_exponent_mask);
    const uint32x4_t n_1 = vshrq_n_u32(two_n, kShiftExponentIntoTopMantissa);
    const uint32x4_t n_0 = vorrq_u32(n_1, vec_eight_biased_exponent);
    const float32x4_t n =
        vsubq_f32(vreinterpretq_f32_u32(n_0),
                  vreinterpretq_f32_u32(vec_implicit_leading_one));
    // Compute y.
    const uint32x4_t vec_mantissa_mask = vdupq_n_u32(0x007FFFFF);
    const uint32x4_t vec_zero_biased_exponent_is_one = vdupq_n_u32(0x3F800000);
    const uint32x4_t mantissa = vandq_u32(vreinterpretq_u32_f32(a),
                                          vec_mantissa_mask);
    const float32x4_t y =
        vreinterpretq_f32_u32(vorrq_u32(mantissa,
                                        vec_zero_biased_exponent_is_one));
    // Approximate log2(y) ~= (y - 1) * pol5(y).
    //    pol5(y) = C5 * y^5 + C4 * y^4 + C3 * y^3 + C2 * y^2 + C1 * y + C0
    const float32x4_t C5 = vdupq_n_f32(-3.4436006e-2f);
    const float32x4_t C4 = vdupq_n_f32(3.1821337e-1f);
    const float32x4_t C3 = vdupq_n_f32(-1.2315303f);
    const float32x4_t C2 = vdupq_n_f32(2.5988452f);
    const float32x4_t C1 = vdupq_n_f32(-3.3241990f);
    const float32x4_t C0 = vdupq_n_f32(3.1157899f);
    float32x4_t pol5_y = C5;
    pol5_y = vmlaq_f32(C4, y, pol5_y);
    pol5_y = vmlaq_f32(C3, y, pol5_y);
    pol5_y = vmlaq_f32(C2, y, pol5_y);
    pol5_y = vmlaq_f32(C1, y, pol5_y);
    pol5_y = vmlaq_f32(C0, y, pol5_y);
    const float32x4_t y_minus_one =
        vsubq_f32(y, vreinterpretq_f32_u32(vec_zero_biased_exponent_is_one));
    const float32x4_t log2_y = vmulq_f32(y_minus_one, pol5_y);

    // Combine parts.
    log2_a = vaddq_f32(n, log2_y);
  }

  // b * log2(a)
  b_log2_a = vmulq_f32(b, log2_a);

  // Calculate exp2(x), x = b * log2(a).
  {
    // To calculate 2^x, we decompose x like this:
    //   x = n + y
    //     n is an integer, the value of x - 0.5 rounded down, therefore
    //     y is in the [0.5, 1.5) range
    //
    //   2^x = 2^n * 2^y
    //     2^n can be evaluated by playing with float representation.
    //     2^y in a small range can be approximated, this code uses an order two
    //         polynomial approximation. The coefficients have been estimated
    //         with the Remez algorithm and the resulting polynomial has a
    //         maximum relative error of 0.17%.
    // To avoid over/underflow, we reduce the range of input to ]-127, 129].
    const float32x4_t max_input = vdupq_n_f32(129.f);
    const float32x4_t min_input = vdupq_n_f32(-126.99999f);
    const float32x4_t x_min = vminq_f32(b_log2_a, max_input);
    const float32x4_t x_max = vmaxq_f32(x_min, min_input);
    // Compute n.
    const float32x4_t half = vdupq_n_f32(0.5f);
    const float32x4_t x_minus_half = vsubq_f32(x_max, half);
    const int32x4_t x_minus_half_floor = vcvtq_s32_f32(x_minus_half);

    // Compute 2^n.
    const int32x4_t float_exponent_bias = vdupq_n_s32(127);
    const int32x4_t two_n_exponent =
        vaddq_s32(x_minus_half_floor, float_exponent_bias);
    const float32x4_t two_n =
        vreinterpretq_f32_s32(vshlq_n_s32(two_n_exponent, kFloatExponentShift));
    // Compute y.
    const float32x4_t y = vsubq_f32(x_max, vcvtq_f32_s32(x_minus_half_floor));

    // Approximate 2^y ~= C2 * y^2 + C1 * y + C0.
    const float32x4_t C2 = vdupq_n_f32(3.3718944e-1f);
    const float32x4_t C1 = vdupq_n_f32(6.5763628e-1f);
    const float32x4_t C0 = vdupq_n_f32(1.0017247f);
    float32x4_t exp2_y = C2;
    exp2_y = vmlaq_f32(C1, y, exp2_y);
    exp2_y = vmlaq_f32(C0, y, exp2_y);

    // Combine parts.
    a_exp_b = vmulq_f32(exp2_y, two_n);
  }

  return a_exp_b;
}

static void OverdriveAndSuppressNEON(AecCore* aec,
                                     float hNl[PART_LEN1],
                                     const float hNlFb,
                                     float efw[2][PART_LEN1]) {
  int i;
  const float32x4_t vec_hNlFb = vmovq_n_f32(hNlFb);
  const float32x4_t vec_one = vdupq_n_f32(1.0f);
  const float32x4_t vec_minus_one = vdupq_n_f32(-1.0f);
  const float32x4_t vec_overDriveSm = vmovq_n_f32(aec->overDriveSm);

  // vectorized code (four at once)
  for (i = 0; i + 3 < PART_LEN1; i += 4) {
    // Weight subbands
    float32x4_t vec_hNl = vld1q_f32(&hNl[i]);
    const float32x4_t vec_weightCurve = vld1q_f32(&WebRtcAec_weightCurve[i]);
    const uint32x4_t bigger = vcgtq_f32(vec_hNl, vec_hNlFb);
    const float32x4_t vec_weightCurve_hNlFb = vmulq_f32(vec_weightCurve,
                                                        vec_hNlFb);
    const float32x4_t vec_one_weightCurve = vsubq_f32(vec_one, vec_weightCurve);
    const float32x4_t vec_one_weightCurve_hNl = vmulq_f32(vec_one_weightCurve,
                                                          vec_hNl);
    const uint32x4_t vec_if0 = vandq_u32(vmvnq_u32(bigger),
                                         vreinterpretq_u32_f32(vec_hNl));
    const float32x4_t vec_one_weightCurve_add =
        vaddq_f32(vec_weightCurve_hNlFb, vec_one_weightCurve_hNl);
    const uint32x4_t vec_if1 =
        vandq_u32(bigger, vreinterpretq_u32_f32(vec_one_weightCurve_add));

    vec_hNl = vreinterpretq_f32_u32(vorrq_u32(vec_if0, vec_if1));

    {
      const float32x4_t vec_overDriveCurve =
          vld1q_f32(&WebRtcAec_overDriveCurve[i]);
      const float32x4_t vec_overDriveSm_overDriveCurve =
          vmulq_f32(vec_overDriveSm, vec_overDriveCurve);
      vec_hNl = vpowq_f32(vec_hNl, vec_overDriveSm_overDriveCurve);
      vst1q_f32(&hNl[i], vec_hNl);
    }

    // Suppress error signal
    {
      float32x4_t vec_efw_re = vld1q_f32(&efw[0][i]);
      float32x4_t vec_efw_im = vld1q_f32(&efw[1][i]);
      vec_efw_re = vmulq_f32(vec_efw_re, vec_hNl);
      vec_efw_im = vmulq_f32(vec_efw_im, vec_hNl);

      // Ooura fft returns incorrect sign on imaginary component. It matters
      // here because we are making an additive change with comfort noise.
      vec_efw_im = vmulq_f32(vec_efw_im, vec_minus_one);
      vst1q_f32(&efw[0][i], vec_efw_re);
      vst1q_f32(&efw[1][i], vec_efw_im);
    }
  }

  // scalar code for the remaining items.
  for (; i < PART_LEN1; i++) {
    // Weight subbands
    if (hNl[i] > hNlFb) {
      hNl[i] = WebRtcAec_weightCurve[i] * hNlFb +
               (1 - WebRtcAec_weightCurve[i]) * hNl[i];
    }

    hNl[i] = powf(hNl[i], aec->overDriveSm * WebRtcAec_overDriveCurve[i]);

    // Suppress error signal
    efw[0][i] *= hNl[i];
    efw[1][i] *= hNl[i];

    // Ooura fft returns incorrect sign on imaginary component. It matters
    // here because we are making an additive change with comfort noise.
    efw[1][i] *= -1;
  }
}

void WebRtcAec_InitAec_neon(void) {
  WebRtcAec_FilterAdaptation = FilterAdaptationNEON;
  WebRtcAec_OverdriveAndSuppress = OverdriveAndSuppressNEON;
}

