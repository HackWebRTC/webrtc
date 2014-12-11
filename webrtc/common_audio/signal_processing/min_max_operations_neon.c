/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <stdlib.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

// Maximum absolute value of word16 vector. C version for generic platforms.
int16_t WebRtcSpl_MaxAbsValueW16Neon(const int16_t* vector, int length) {
  int absolute = 0, maximum = 0;

  if (vector == NULL || length <= 0) {
    return -1;
  }

  const int16_t* p_start = vector;
  int rest = length & 7;
  const int16_t* p_end = vector + length - rest;

  int16x8_t v;
  uint16x8_t max_qv;
  max_qv = vdupq_n_u16(0);

  while (p_start < p_end) {
    v = vld1q_s16(p_start);
    // Note vabs doesn't change the value of -32768.
    v = vabsq_s16(v);
    // Use u16 so we don't lose the value -32768.
    max_qv = vmaxq_u16(max_qv, vreinterpretq_u16_s16(v));
    p_start += 8;
  }

#ifdef WEBRTC_ARCH_ARM64
  maximum = (int)vmaxvq_u16(max_qv);
#else
  uint16x4_t max_dv;
  max_dv = vmax_u16(vget_low_u16(max_qv), vget_high_u16(max_qv));
  max_dv = vpmax_u16(max_dv, max_dv);
  max_dv = vpmax_u16(max_dv, max_dv);

  maximum = (int)vget_lane_u16(max_dv, 0);
#endif

  p_end = vector + length;
  while (p_start < p_end) {
    absolute = abs((int)(*p_start));

    if (absolute > maximum) {
      maximum = absolute;
    }
    p_start++;
  }

  // Guard the case for abs(-32768).
  if (maximum > WEBRTC_SPL_WORD16_MAX) {
    maximum = WEBRTC_SPL_WORD16_MAX;
  }

  return (int16_t)maximum;
}

// Minimum value of word16 vector. NEON intrinsics version for
// ARM 32-bit/64-bit platforms.
int16_t WebRtcSpl_MinValueW16Neon(const int16_t* vector, int length) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  int i = 0;
  int residual = length & 0x7;

  if (vector == NULL || length <= 0) {
    return minimum;
  }

  const int16_t* p_start = vector;
  int16x8_t min16x8 = vdupq_n_s16(WEBRTC_SPL_WORD16_MAX);

  // First part, unroll the loop 8 times.
  for (i = length - residual; i >0; i -= 8) {
    int16x8_t in16x8 = vld1q_s16(p_start);
    min16x8 = vminq_s16(min16x8, in16x8);
    p_start += 8;
  }

#if defined(WEBRTC_ARCH_ARM64)
  minimum = vminvq_s16(min16x8);
#else
  int16x4_t min16x4 = vmin_s16(vget_low_s16(min16x8), vget_high_s16(min16x8));
  min16x4 = vpmin_s16(min16x4, min16x4);
  min16x4 = vpmin_s16(min16x4, min16x4);

  minimum = vget_lane_s16(min16x4, 0);
#endif

  // Second part, do the remaining iterations (if any).
  for (i = residual; i > 0; i--) {
    if (*p_start < minimum)
      minimum = *p_start;
    p_start++;
  }
  return minimum;
}

