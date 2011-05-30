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
    const WebRtc_Word16 *inptr;
    WebRtc_Word16 *outptr;
    WebRtc_Word32 *state;
    WebRtc_Word32 tmp1, tmp2, diff, in32, out32;
    WebRtc_Word16 i;

    // local versions of pointers to input and output arrays
    inptr = in; // input array
    outptr = out; // output array (of length len/2)
    state = filtState; // filter state array; length = 8

    for (i = (len >> 1); i > 0; i--)
    {
        // lower allpass filter
        in32 = (WebRtc_Word32)(*inptr++) << 10;
        diff = in32 - state[1];
        tmp1 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[0], diff, state[0] );
        state[0] = in32;
        diff = tmp1 - state[2];
        tmp2 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[1], diff, state[1] );
        state[1] = tmp1;
        diff = tmp2 - state[3];
        state[3] = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[2], diff, state[2] );
        state[2] = tmp2;

        // upper allpass filter
        in32 = (WebRtc_Word32)(*inptr++) << 10;
        diff = in32 - state[5];
        tmp1 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[0], diff, state[4] );
        state[4] = in32;
        diff = tmp1 - state[6];
        tmp2 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[1], diff, state[5] );
        state[5] = tmp1;
        diff = tmp2 - state[7];
        state[7] = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[2], diff, state[6] );
        state[6] = tmp2;

        // add two allpass outputs, divide by two and round
        out32 = (state[3] + state[7] + 1024) >> 11;

        // limit amplitude to prevent wrap-around, and write to output array
        if (out32 > 32767)
            *outptr++ = 32767;
        else if (out32 < -32768)
            *outptr++ = -32768;
        else
            *outptr++ = (WebRtc_Word16)out32;
    }
}

void WebRtcSpl_UpsampleBy2(const WebRtc_Word16* in, WebRtc_Word16 len, WebRtc_Word16* out,
                           WebRtc_Word32* filtState)
{
    const WebRtc_Word16 *inptr;
    WebRtc_Word16 *outptr;
    WebRtc_Word32 *state;
    WebRtc_Word32 tmp1, tmp2, diff, in32, out32;
    WebRtc_Word16 i;

    // local versions of pointers to input and output arrays
    inptr = in; // input array
    outptr = out; // output array (of length len*2)
    state = filtState; // filter state array; length = 8

    for (i = len; i > 0; i--)
    {
        // lower allpass filter
        in32 = (WebRtc_Word32)(*inptr++) << 10;
        diff = in32 - state[1];
        tmp1 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[0], diff, state[0] );
        state[0] = in32;
        diff = tmp1 - state[2];
        tmp2 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[1], diff, state[1] );
        state[1] = tmp1;
        diff = tmp2 - state[3];
        state[3] = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass1[2], diff, state[2] );
        state[2] = tmp2;

        // round; limit amplitude to prevent wrap-around; write to output array
        out32 = (state[3] + 512) >> 10;
        if (out32 > 32767)
            *outptr++ = 32767;
        else if (out32 < -32768)
            *outptr++ = -32768;
        else
            *outptr++ = (WebRtc_Word16)out32;

        // upper allpass filter
        diff = in32 - state[5];
        tmp1 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[0], diff, state[4] );
        state[4] = in32;
        diff = tmp1 - state[6];
        tmp2 = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[1], diff, state[5] );
        state[5] = tmp1;
        diff = tmp2 - state[7];
        state[7] = WEBRTC_SPL_SCALEDIFF32( kResampleAllpass2[2], diff, state[6] );
        state[6] = tmp2;

        // round; limit amplitude to prevent wrap-around; write to output array
        out32 = (state[7] + 512) >> 10;
        if (out32 > 32767)
            *outptr++ = 32767;
        else if (out32 < -32768)
            *outptr++ = -32768;
        else
            *outptr++ = (WebRtc_Word16)out32;
    }
}
