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
 * This file contains the resampling by two functions.
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

// allpass filter coefficients.
static const WebRtc_UWord16 kResampleAllpass1[3] = {3284, 24441, 49528};
static const WebRtc_UWord16 kResampleAllpass2[3] = {12199, 37471, 60255};

// decimator
void WebRtcSpl_DownsampleBy2(const WebRtc_Word16* in, const WebRtc_Word16 len,
                             WebRtc_Word16* out, WebRtc_Word32* filtState)
{
    WebRtc_Word32 tmp1, tmp2, diff, in32, out32;
    WebRtc_Word16 i;

    register WebRtc_Word32 state0 = filtState[0];
    register WebRtc_Word32 state1 = filtState[1];
    register WebRtc_Word32 state2 = filtState[2];
    register WebRtc_Word32 state3 = filtState[3];
    register WebRtc_Word32 state4 = filtState[4];
    register WebRtc_Word32 state5 = filtState[5];
    register WebRtc_Word32 state6 = filtState[6];
    register WebRtc_Word32 state7 = filtState[7];

    for (i = (len >> 1); i > 0; i--)
    {
        // lower allpass filter
        in32 = (WebRtc_Word32)(*in++) << 10;
        diff = in32 - state1;
        tmp1 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[0], diff, state0);
        state0 = in32;
        diff = tmp1 - state2;
        tmp2 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[1], diff, state1);
        state1 = tmp1;
        diff = tmp2 - state3;
        state3 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[2], diff, state2);
        state2 = tmp2;

        // upper allpass filter
        in32 = (WebRtc_Word32)(*in++) << 10;
        diff = in32 - state5;
        tmp1 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[0], diff, state4);
        state4 = in32;
        diff = tmp1 - state6;
        tmp2 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[1], diff, state5);
        state5 = tmp1;
        diff = tmp2 - state7;
        state7 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[2], diff, state6);
        state6 = tmp2;

        // add two allpass outputs, divide by two and round
        out32 = (state3 + state7 + 1024) >> 11;

        // limit amplitude to prevent wrap-around, and write to output array
#ifdef WEBRTC_ARCH_ARM_V7A
        __asm__("ssat %r0, #16, %r1" : "=r"(*out) : "r"(out32));
        out++;
#else
        if (out32 > 32767)
            *out++ = 32767;
        else if (out32 < -32768)
            *out++ = -32768;
        else
            *out++ = (WebRtc_Word16)out32;
#endif
    }

    filtState[0] = state0;
    filtState[1] = state1;
    filtState[2] = state2;
    filtState[3] = state3;
    filtState[4] = state4;
    filtState[5] = state5;
    filtState[6] = state6;
    filtState[7] = state7;
}

void WebRtcSpl_UpsampleBy2(const WebRtc_Word16* in, WebRtc_Word16 len, WebRtc_Word16* out,
                           WebRtc_Word32* filtState)
{
    WebRtc_Word32 tmp1, tmp2, diff, in32, out32;
    WebRtc_Word16 i;

    register WebRtc_Word32 state0 = filtState[0];
    register WebRtc_Word32 state1 = filtState[1];
    register WebRtc_Word32 state2 = filtState[2];
    register WebRtc_Word32 state3 = filtState[3];
    register WebRtc_Word32 state4 = filtState[4];
    register WebRtc_Word32 state5 = filtState[5];
    register WebRtc_Word32 state6 = filtState[6];
    register WebRtc_Word32 state7 = filtState[7];

    for (i = len; i > 0; i--)
    {
        // lower allpass filter
        in32 = (WebRtc_Word32)(*in++) << 10;
        diff = in32 - state1;
        tmp1 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[0], diff, state0);
        state0 = in32;
        diff = tmp1 - state2;
        tmp2 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[1], diff, state1);
        state1 = tmp1;
        diff = tmp2 - state3;
        state3 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass1[2], diff, state2);
        state2 = tmp2;

        // round; limit amplitude to prevent wrap-around; write to output array
        out32 = (state3 + 512) >> 10;
#ifdef WEBRTC_ARCH_ARM_V7A
        __asm__("ssat %r0, #16, %r1":"=r"(*out): "r"(out32));
        out++;
#else
        if (out32 > 32767)
            *out++ = 32767;
        else if (out32 < -32768)
            *out++ = -32768;
        else
            *out++ = (WebRtc_Word16)out32;
#endif

        // upper allpass filter
        diff = in32 - state5;
        tmp1 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[0], diff, state4);
        state4 = in32;
        diff = tmp1 - state6;
        tmp2 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[1], diff, state5);
        state5 = tmp1;
        diff = tmp2 - state7;
        state7 = WEBRTC_SPL_SCALEDIFF32(kResampleAllpass2[2], diff, state6);
        state6 = tmp2;

        // round; limit amplitude to prevent wrap-around; write to output array
        out32 = (state7 + 512) >> 10;
#ifdef WEBRTC_ARCH_ARM_V7A
        __asm__("ssat %r0, #16, %r1":"=r"(*out): "r"(out32));
        out++;
#else
        if (out32 > 32767)
            *out++ = 32767;
        else if (out32 < -32768)
            *out++ = -32768;
        else
            *out++ = (WebRtc_Word16)out32;
#endif
    }
    
    filtState[0] = state0;
    filtState[1] = state1;
    filtState[2] = state2;
    filtState[3] = state3;
    filtState[4] = state4;
    filtState[5] = state5;
    filtState[6] = state6;
    filtState[7] = state7;
}
