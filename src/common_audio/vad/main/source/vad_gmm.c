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
 * This file includes the implementation of the internal VAD call
 * WebRtcVad_GaussianProbability. For function description, see vad_gmm.h.
 */

#include "vad_gmm.h"

#include "signal_processing_library.h"
#include "typedefs.h"

static const WebRtc_Word32 kCompVar = 22005;
// Constant log2(exp(1)) in Q12
static const WebRtc_Word16 kLog10Const = 5909;

WebRtc_Word32 WebRtcVad_GaussianProbability(WebRtc_Word16 in_sample,
                                            WebRtc_Word16 mean,
                                            WebRtc_Word16 std,
                                            WebRtc_Word16 *delta)
{
    WebRtc_Word16 tmp16, tmpDiv, tmpDiv2, expVal, tmp16_1, tmp16_2;
    WebRtc_Word32 tmp32, y32;

    // Calculate tmpDiv=1/std, in Q10
    tmp32 = (WebRtc_Word32)WEBRTC_SPL_RSHIFT_W16(std,1) + (WebRtc_Word32)131072; // 1 in Q17
    tmpDiv = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32, std); // Q17/Q7 = Q10

    // Calculate tmpDiv2=1/std^2, in Q14
    tmp16 = WEBRTC_SPL_RSHIFT_W16(tmpDiv, 2); // From Q10 to Q8
    tmpDiv2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16, tmp16, 2); // (Q8 * Q8)>>2 = Q14

    tmp16 = WEBRTC_SPL_LSHIFT_W16(in_sample, 3); // Q7
    tmp16 = tmp16 - mean; // Q7 - Q7 = Q7

    // To be used later, when updating noise/speech model
    // delta = (x-m)/std^2, in Q11
    *delta = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmpDiv2, tmp16, 10); //(Q14*Q7)>>10 = Q11

    // Calculate tmp32=(x-m)^2/(2*std^2), in Q10
    tmp32 = (WebRtc_Word32)WEBRTC_SPL_MUL_16_16_RSFT(*delta, tmp16, 9); // One shift for /2

    // Calculate expVal ~= exp(-(x-m)^2/(2*std^2)) ~= exp2(-log2(exp(1))*tmp32)
    if (tmp32 < kCompVar)
    {
        // Calculate tmp16 = log2(exp(1))*tmp32 , in Q10
        tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16)tmp32,
                                                         kLog10Const, 12);
        tmp16 = -tmp16;
        tmp16_2 = (WebRtc_Word16)(0x0400 | (tmp16 & 0x03FF));
        tmp16_1 = (WebRtc_Word16)(tmp16 ^ 0xFFFF);
        tmp16 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(tmp16_1, 10);
        tmp16 += 1;
        // Calculate expVal=log2(-tmp32), in Q10
        expVal = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32)tmp16_2, tmp16);

    } else
    {
        expVal = 0;
    }

    // Calculate y32=(1/std)*exp(-(x-m)^2/(2*std^2)), in Q20
    y32 = WEBRTC_SPL_MUL_16_16(tmpDiv, expVal); // Q10 * Q10 = Q20

    return y32; // Q20
}
