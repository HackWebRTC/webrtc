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
 * This file includes the implementation of the internal filterbank associated functions.
 * For function description, see vad_filterbank.h.
 */

#include "vad_filterbank.h"

#include "signal_processing_library.h"
#include "typedefs.h"
#include "vad_defines.h"

// Constant 160*log10(2) in Q9
static const WebRtc_Word16 kLogConst = 24660;
// Coefficients used by WebRtcVad_HpOutput, Q14
static const WebRtc_Word16 kHpZeroCoefs[3] = {6631, -13262, 6631};
static const WebRtc_Word16 kHpPoleCoefs[3] = {16384, -7756, 5620};
// Allpass filter coefficients, upper and lower, in Q15
// Upper: 0.64, Lower: 0.17
static const WebRtc_Word16 kAllPassCoefsQ15[2] = {20972, 5571};
// Adjustment for division with two in WebRtcVad_SplitFilter
static const WebRtc_Word16 kOffsetVector[6] = {368, 368, 272, 176, 176, 176};

void WebRtcVad_HpOutput(WebRtc_Word16 *in_vector,
                        WebRtc_Word16 in_vector_length,
                        WebRtc_Word16 *out_vector,
                        WebRtc_Word16 *filter_state)
{
    WebRtc_Word16 i, *pi, *outPtr;
    WebRtc_Word32 tmpW32;

    pi = &in_vector[0];
    outPtr = &out_vector[0];

    // The sum of the absolute values of the impulse response:
    // The zero/pole-filter has a max amplification of a single sample of: 1.4546
    // Impulse response: 0.4047 -0.6179 -0.0266  0.1993  0.1035  -0.0194
    // The all-zero section has a max amplification of a single sample of: 1.6189
    // Impulse response: 0.4047 -0.8094  0.4047  0       0        0
    // The all-pole section has a max amplification of a single sample of: 1.9931
    // Impulse response: 1.0000  0.4734 -0.1189 -0.2187 -0.0627   0.04532

    for (i = 0; i < in_vector_length; i++)
    {
        // all-zero section (filter coefficients in Q14)
        tmpW32 = (WebRtc_Word32)WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[0], (*pi));
        tmpW32 += (WebRtc_Word32)WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[1], filter_state[0]);
        tmpW32 += (WebRtc_Word32)WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[2], filter_state[1]); // Q14
        filter_state[1] = filter_state[0];
        filter_state[0] = *pi++;

        // all-pole section
        tmpW32 -= (WebRtc_Word32)WEBRTC_SPL_MUL_16_16(kHpPoleCoefs[1], filter_state[2]); // Q14
        tmpW32 -= (WebRtc_Word32)WEBRTC_SPL_MUL_16_16(kHpPoleCoefs[2], filter_state[3]);
        filter_state[3] = filter_state[2];
        filter_state[2] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32 (tmpW32, 14);
        *outPtr++ = filter_state[2];
    }
}

void WebRtcVad_Allpass(WebRtc_Word16 *in_vector,
                       WebRtc_Word16 *out_vector,
                       WebRtc_Word16 filter_coefficients,
                       int vector_length,
                       WebRtc_Word16 *filter_state)
{
    // The filter can only cause overflow (in the w16 output variable)
    // if more than 4 consecutive input numbers are of maximum value and
    // has the the same sign as the impulse responses first taps.
    // First 6 taps of the impulse response: 0.6399 0.5905 -0.3779
    // 0.2418 -0.1547 0.0990

    int n;
    WebRtc_Word16 tmp16;
    WebRtc_Word32 tmp32, in32, state32;

    state32 = WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)(*filter_state)), 16); // Q31

    for (n = 0; n < vector_length; n++)
    {

        tmp32 = state32 + WEBRTC_SPL_MUL_16_16(filter_coefficients, (*in_vector));
        tmp16 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32, 16);
        *out_vector++ = tmp16;
        in32 = WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)(*in_vector)), 14);
        state32 = in32 - WEBRTC_SPL_MUL_16_16(filter_coefficients, tmp16);
        state32 = WEBRTC_SPL_LSHIFT_W32(state32, 1);
        in_vector += 2;
    }

    *filter_state = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(state32, 16);
}

void WebRtcVad_SplitFilter(WebRtc_Word16 *in_vector,
                           WebRtc_Word16 *out_vector_hp,
                           WebRtc_Word16 *out_vector_lp,
                           WebRtc_Word16 *upper_state,
                           WebRtc_Word16 *lower_state,
                           int in_vector_length)
{
    WebRtc_Word16 tmpOut;
    int k, halflen;

    // Downsampling by 2 and get two branches
    halflen = WEBRTC_SPL_RSHIFT_W16(in_vector_length, 1);

    // All-pass filtering upper branch
    WebRtcVad_Allpass(&in_vector[0], out_vector_hp, kAllPassCoefsQ15[0], halflen, upper_state);

    // All-pass filtering lower branch
    WebRtcVad_Allpass(&in_vector[1], out_vector_lp, kAllPassCoefsQ15[1], halflen, lower_state);

    // Make LP and HP signals
    for (k = 0; k < halflen; k++)
    {
        tmpOut = *out_vector_hp;
        *out_vector_hp++ -= *out_vector_lp;
        *out_vector_lp++ += tmpOut;
    }
}

WebRtc_Word16 WebRtcVad_get_features(VadInstT *inst,
                                     WebRtc_Word16 *in_vector,
                                     int frame_size,
                                     WebRtc_Word16 *out_vector)
{
    int curlen, filtno;
    WebRtc_Word16 vecHP1[120], vecLP1[120];
    WebRtc_Word16 vecHP2[60], vecLP2[60];
    WebRtc_Word16 *ptin;
    WebRtc_Word16 *hptout, *lptout;
    WebRtc_Word16 power = 0;

    // Split at 2000 Hz and downsample
    filtno = 0;
    ptin = in_vector;
    hptout = vecHP1;
    lptout = vecLP1;
    curlen = frame_size;
    WebRtcVad_SplitFilter(ptin, hptout, lptout, &inst->upper_state[filtno],
                  &inst->lower_state[filtno], curlen);

    // Split at 3000 Hz and downsample
    filtno = 1;
    ptin = vecHP1;
    hptout = vecHP2;
    lptout = vecLP2;
    curlen = WEBRTC_SPL_RSHIFT_W16(frame_size, 1);

    WebRtcVad_SplitFilter(ptin, hptout, lptout, &inst->upper_state[filtno],
                  &inst->lower_state[filtno], curlen);

    // Energy in 3000 Hz - 4000 Hz
    curlen = WEBRTC_SPL_RSHIFT_W16(curlen, 1);
    WebRtcVad_LogOfEnergy(vecHP2, &out_vector[5], &power, kOffsetVector[5], curlen);

    // Energy in 2000 Hz - 3000 Hz
    WebRtcVad_LogOfEnergy(vecLP2, &out_vector[4], &power, kOffsetVector[4], curlen);

    // Split at 1000 Hz and downsample
    filtno = 2;
    ptin = vecLP1;
    hptout = vecHP2;
    lptout = vecLP2;
    curlen = WEBRTC_SPL_RSHIFT_W16(frame_size, 1);
    WebRtcVad_SplitFilter(ptin, hptout, lptout, &inst->upper_state[filtno],
                  &inst->lower_state[filtno], curlen);

    // Energy in 1000 Hz - 2000 Hz
    curlen = WEBRTC_SPL_RSHIFT_W16(curlen, 1);
    WebRtcVad_LogOfEnergy(vecHP2, &out_vector[3], &power, kOffsetVector[3], curlen);

    // Split at 500 Hz
    filtno = 3;
    ptin = vecLP2;
    hptout = vecHP1;
    lptout = vecLP1;

    WebRtcVad_SplitFilter(ptin, hptout, lptout, &inst->upper_state[filtno],
                  &inst->lower_state[filtno], curlen);

    // Energy in 500 Hz - 1000 Hz
    curlen = WEBRTC_SPL_RSHIFT_W16(curlen, 1);
    WebRtcVad_LogOfEnergy(vecHP1, &out_vector[2], &power, kOffsetVector[2], curlen);
    // Split at 250 Hz
    filtno = 4;
    ptin = vecLP1;
    hptout = vecHP2;
    lptout = vecLP2;

    WebRtcVad_SplitFilter(ptin, hptout, lptout, &inst->upper_state[filtno],
                  &inst->lower_state[filtno], curlen);

    // Energy in 250 Hz - 500 Hz
    curlen = WEBRTC_SPL_RSHIFT_W16(curlen, 1);
    WebRtcVad_LogOfEnergy(vecHP2, &out_vector[1], &power, kOffsetVector[1], curlen);

    // Remove DC and LFs
    WebRtcVad_HpOutput(vecLP2, curlen, vecHP1, inst->hp_filter_state);

    // Power in 80 Hz - 250 Hz
    WebRtcVad_LogOfEnergy(vecHP1, &out_vector[0], &power, kOffsetVector[0], curlen);

    return power;
}

void WebRtcVad_LogOfEnergy(WebRtc_Word16 *vector,
                           WebRtc_Word16 *enerlogval,
                           WebRtc_Word16 *power,
                           WebRtc_Word16 offset,
                           int vector_length)
{
    WebRtc_Word16 enerSum = 0;
    WebRtc_Word16 zeros, frac, log2;
    WebRtc_Word32 energy;

    int shfts = 0, shfts2;

    energy = WebRtcSpl_Energy(vector, vector_length, &shfts);

    if (energy > 0)
    {

        shfts2 = 16 - WebRtcSpl_NormW32(energy);
        shfts += shfts2;
        // "shfts" is the total number of right shifts that has been done to enerSum.
        enerSum = (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(energy, -shfts2);

        // Find:
        // 160*log10(enerSum*2^shfts) = 160*log10(2)*log2(enerSum*2^shfts) =
        // 160*log10(2)*(log2(enerSum) + log2(2^shfts)) =
        // 160*log10(2)*(log2(enerSum) + shfts)

        zeros = WebRtcSpl_NormU32(enerSum);
        frac = (WebRtc_Word16)(((WebRtc_UWord32)((WebRtc_Word32)(enerSum) << zeros)
                & 0x7FFFFFFF) >> 21);
        log2 = (WebRtc_Word16)(((31 - zeros) << 10) + frac);

        *enerlogval = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(kLogConst, log2, 19)
                + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(shfts, kLogConst, 9);

        if (*enerlogval < 0)
        {
            *enerlogval = 0;
        }
    } else
    {
        *enerlogval = 0;
        shfts = -15;
        enerSum = 0;
    }

    *enerlogval += offset;

    // Total power in frame
    if (*power <= MIN_ENERGY)
    {
        if (shfts > 0)
        {
            *power += MIN_ENERGY + 1;
        } else if (WEBRTC_SPL_SHIFT_W16(enerSum, shfts) > MIN_ENERGY)
        {
            *power += MIN_ENERGY + 1;
        } else
        {
            *power += WEBRTC_SPL_SHIFT_W16(enerSum, shfts);
        }
    }
}
