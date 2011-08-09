/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_ARCH_ARM_NEON) && defined(WEBRTC_ANDROID)

#include "nsx_core.h"
#include <arm_neon.h>

void WebRtcNsx_NoiseEstimation(NsxInst_t *inst, WebRtc_UWord16 *magn, WebRtc_UWord32 *noise,
                               WebRtc_Word16 *qNoise)
{
    WebRtc_Word32 numerator;

    WebRtc_Word16 lmagn[HALF_ANAL_BLOCKL], counter, countDiv, countProd, delta, zeros, frac;
    WebRtc_Word16 log2, tabind, logval, tmp16, tmp16no1, tmp16no2;
    WebRtc_Word16 log2Const = 22713;
    WebRtc_Word16 widthFactor = 21845;

    int i, s, offset;

    numerator = FACTOR_Q16;

    tabind = inst->stages - inst->normData;
    if (tabind < 0)
    {
        logval = -WebRtcNsx_kLogTable[-tabind];
    } else
    {
        logval = WebRtcNsx_kLogTable[tabind];
    }

    // lmagn(i)=log(magn(i))=log(2)*log2(magn(i))
    // magn is in Q(-stages), and the real lmagn values are:
    // real_lmagn(i)=log(magn(i)*2^stages)=log(magn(i))+log(2^stages)
    // lmagn in Q8
    for (i = 0; i < inst->magnLen; i++)
    {
        if (magn[i])
        {
            zeros = WebRtcSpl_NormU32((WebRtc_UWord32)magn[i]);
            frac = (WebRtc_Word16)((((WebRtc_UWord32)magn[i] << zeros) & 0x7FFFFFFF) >> 23);
            // log2(magn(i))
            log2 = (WebRtc_Word16)(((31 - zeros) << 8) + WebRtcNsx_kLogTableFrac[frac]);
            // log2(magn(i))*log(2)
            lmagn[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(log2, log2Const, 15);
            // + log(2^stages)
            lmagn[i] += logval;
        } else
        {
            lmagn[i] = logval;
        }
    }

    int16x4_t Q3_16x4  = vdup_n_s16(3);
    int16x8_t WIDTHQ8_16x8 = vdupq_n_s16(WIDTH_Q8);
    int16x8_t WIDTHFACTOR_16x8 = vdupq_n_s16(widthFactor);

    // Loop over simultaneous estimates
    for (s = 0; s < SIMULT; s++)
    {
        offset = s * inst->magnLen;

        // Get counter values from state
        counter = inst->noiseEstCounter[s];
        countDiv = WebRtcNsx_kCounterDiv[counter];
        countProd = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(counter, countDiv);

        // quant_est(...)
        WebRtc_Word16 delta_[8];
        int16x4_t tmp16x4_0;
        int16x4_t tmp16x4_1;
        int16x4_t countDiv_16x4 = vdup_n_s16(countDiv);
        int16x8_t countProd_16x8 = vdupq_n_s16(countProd);
        int16x8_t tmp16x8_0 = vdupq_n_s16(countDiv);
        int16x8_t prod16x8 = vqrdmulhq_s16(WIDTHFACTOR_16x8, tmp16x8_0);
        int16x8_t tmp16x8_1;
        int16x8_t tmp16x8_2;
        int16x8_t tmp16x8_3;
        int16x8_t tmp16x8_4;
        int16x8_t tmp16x8_5;
        int32x4_t tmp32x4;

        for (i = 0; i < inst->magnLen - 7; i += 8) {
            // compute delta
            tmp16x8_0 = vdupq_n_s16(FACTOR_Q7);
            vst1q_s16(delta_, tmp16x8_0);
            int j;
            for (j = 0; j < 8; j++) {
                if (inst->noiseEstDensity[offset + i + j] > 512)
                    delta_[j] = WebRtcSpl_DivW32W16ResW16(numerator, 
                                   inst->noiseEstDensity[offset + i + j]);
            }

            // Update log quantile estimate

            // tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(delta, countDiv, 14);
            tmp32x4 = vmull_s16(vld1_s16(&delta_[0]), countDiv_16x4);
            tmp16x4_1 = vshrn_n_s32(tmp32x4, 14);
            tmp32x4 = vmull_s16(vld1_s16(&delta_[4]), countDiv_16x4);
            tmp16x4_0 = vshrn_n_s32(tmp32x4, 14);
            tmp16x8_0 = vcombine_s16(tmp16x4_1, tmp16x4_0); // Keep for several lines.

            // prepare for the "if" branch
            // tmp16 += 2;
            // tmp16_1 = (Word16)(tmp16>>2);
            tmp16x8_1 = vrshrq_n_s16(tmp16x8_0, 2);

            // inst->noiseEstLogQuantile[offset+i] + tmp16_1;
            tmp16x8_2 = vld1q_s16(&inst->noiseEstLogQuantile[offset + i]); // Keep
            tmp16x8_1 = vaddq_s16(tmp16x8_2, tmp16x8_1); // Keep for several lines

            // Prepare for the "else" branch
            // tmp16 += 1;
            // tmp16_1 = (Word16)(tmp16>>1);
            tmp16x8_0 = vrshrq_n_s16(tmp16x8_0, 1);

            // tmp16_2 = (Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16_1,3,1);
            tmp32x4 = vmull_s16(vget_low_s16(tmp16x8_0), Q3_16x4);
            tmp16x4_1 = vshrn_n_s32(tmp32x4, 1);

            // tmp16_2 = (Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16_1,3,1);
            tmp32x4 = vmull_s16(vget_high_s16(tmp16x8_0), Q3_16x4);
            tmp16x4_0 = vshrn_n_s32(tmp32x4, 1);

            // inst->noiseEstLogQuantile[offset + i] - tmp16_2;
            tmp16x8_0 = vcombine_s16(tmp16x4_1, tmp16x4_0); // keep
            tmp16x8_0 = vsubq_s16(tmp16x8_2, tmp16x8_0);

            // Do the if-else branches:
            tmp16x8_3 = vld1q_s16(&lmagn[i]); // keep for several lines
            tmp16x8_5 = vsubq_s16(tmp16x8_3, tmp16x8_2);
            __asm__("vcgt.s16 %q0, %q1, #0"::"w"(tmp16x8_4), "w"(tmp16x8_5));
            __asm__("vbit %q0, %q1, %q2"::"w"(tmp16x8_2), "w"(tmp16x8_1), "w"(tmp16x8_4));
            __asm__("vbif %q0, %q1, %q2"::"w"(tmp16x8_2), "w"(tmp16x8_0), "w"(tmp16x8_4));
            vst1q_s16(&inst->noiseEstLogQuantile[offset + i], tmp16x8_2);

            // Update density estimate
            // tmp16_1 + tmp16_2
            tmp16x8_1 = vld1q_s16(&inst->noiseEstDensity[offset + i]);
            tmp16x8_0 = vqrdmulhq_s16(tmp16x8_1, countProd_16x8);
            tmp16x8_0 = vaddq_s16(tmp16x8_0, prod16x8);

            // lmagn[i] - inst->noiseEstLogQuantile[offset + i]
            tmp16x8_3 = vsubq_s16(tmp16x8_3, tmp16x8_2);
            tmp16x8_3 = vabsq_s16(tmp16x8_3);
            tmp16x8_4 = vcgtq_s16(WIDTHQ8_16x8, tmp16x8_3);
            __asm__("vbit %q0, %q1, %q2"::"w"(tmp16x8_1), "w"(tmp16x8_0), "w"(tmp16x8_4));
            vst1q_s16(&inst->noiseEstDensity[offset + i], tmp16x8_1);
        } // End loop over magnitude spectrum

        for (; i < inst->magnLen; i++)
        {
            // compute delta
            if (inst->noiseEstDensity[offset + i] > 512)
            {
                delta = WebRtcSpl_DivW32W16ResW16(numerator,
                                                  inst->noiseEstDensity[offset + i]);
            } else
            {
                delta = FACTOR_Q7;
            }

            // update log quantile estimate
            tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(delta, countDiv, 14);
            if (lmagn[i] > inst->noiseEstLogQuantile[offset + i])
            {
                // +=QUANTILE*delta/(inst->counter[s]+1) QUANTILE=0.25, =1 in Q2
                // CounterDiv=1/inst->counter[s] in Q15
                tmp16 += 2;
                tmp16no1 = WEBRTC_SPL_RSHIFT_W16(tmp16, 2);
                inst->noiseEstLogQuantile[offset + i] += tmp16no1;
            } else
            {
                tmp16 += 1;
                tmp16no1 = WEBRTC_SPL_RSHIFT_W16(tmp16, 1);
                // *(1-QUANTILE), in Q2 QUANTILE=0.25, 1-0.25=0.75=3 in Q2
                tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16no1, 3, 1);
                inst->noiseEstLogQuantile[offset + i] -= tmp16no2;
            }

            // update density estimate
            if (WEBRTC_SPL_ABS_W16(lmagn[i] - inst->noiseEstLogQuantile[offset + i])
                    < WIDTH_Q8)
            {
                tmp16no1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
                        inst->noiseEstDensity[offset + i], countProd, 15);
                tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(widthFactor,
                                                                               countDiv, 15);
                inst->noiseEstDensity[offset + i] = tmp16no1 + tmp16no2;
            }
        } // end loop over magnitude spectrum

        if (counter >= END_STARTUP_LONG)
        {
            inst->noiseEstCounter[s] = 0;
            if (inst->blockIndex >= END_STARTUP_LONG)
            {
                WebRtcNsx_UpdateNoiseEstimate(inst, offset);
            }
        }
        inst->noiseEstCounter[s]++;

    } // end loop over simultaneous estimates

    // Sequentially update the noise during startup
    if (inst->blockIndex < END_STARTUP_LONG)
    {
        WebRtcNsx_UpdateNoiseEstimate(inst, offset);
    }

    for (i = 0; i < inst->magnLen; i++)
    {
        noise[i] = (WebRtc_UWord32)(inst->noiseEstQuantile[i]); // Q(qNoise)
    }
    (*qNoise) = (WebRtc_Word16)inst->qNoise;
}

#endif // defined(WEBRTC_ARCH_ARM_NEON) && defined(WEBRTC_ANDROID)
