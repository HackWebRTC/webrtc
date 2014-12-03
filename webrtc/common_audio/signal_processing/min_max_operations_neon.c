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

