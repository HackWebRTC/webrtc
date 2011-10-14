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
 * This file includes the implementation of the VAD internal calls for
 * Downsampling and FindMinimum.
 * For function call descriptions; See vad_sp.h.
 */

#include "vad_sp.h"

#include "signal_processing_library.h"
#include "typedefs.h"
#include "vad_defines.h"

// Allpass filter coefficients, upper and lower, in Q13
// Upper: 0.64, Lower: 0.17
static const WebRtc_Word16 kAllPassCoefsQ13[2] = {5243, 1392}; // Q13

// Downsampling filter based on the splitting filter and the allpass functions
// in vad_filterbank.c
void WebRtcVad_Downsampling(WebRtc_Word16* signal_in,
                            WebRtc_Word16* signal_out,
                            WebRtc_Word32* filter_state,
                            int inlen)
{
    WebRtc_Word16 tmp16_1, tmp16_2;
    WebRtc_Word32 tmp32_1, tmp32_2;
    int n, halflen;

    // Downsampling by 2 and get two branches
    halflen = WEBRTC_SPL_RSHIFT_W16(inlen, 1);

    tmp32_1 = filter_state[0];
    tmp32_2 = filter_state[1];

    // Filter coefficients in Q13, filter state in Q0
    for (n = 0; n < halflen; n++)
    {
        // All-pass filtering upper branch
        tmp16_1 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32_1, 1)
                + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((kAllPassCoefsQ13[0]),
                                                           *signal_in, 14);
        *signal_out = tmp16_1;
        tmp32_1 = (WebRtc_Word32)(*signal_in++)
                - (WebRtc_Word32)WEBRTC_SPL_MUL_16_16_RSFT((kAllPassCoefsQ13[0]), tmp16_1, 12);

        // All-pass filtering lower branch
        tmp16_2 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32_2, 1)
                + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((kAllPassCoefsQ13[1]),
                                                           *signal_in, 14);
        *signal_out++ += tmp16_2;
        tmp32_2 = (WebRtc_Word32)(*signal_in++)
                - (WebRtc_Word32)WEBRTC_SPL_MUL_16_16_RSFT((kAllPassCoefsQ13[1]), tmp16_2, 12);
    }
    filter_state[0] = tmp32_1;
    filter_state[1] = tmp32_2;
}

WebRtc_Word16 WebRtcVad_FindMinimum(VadInstT* inst,
                                    WebRtc_Word16 x,
                                    int n)
{
    int i, j, k, II = -1, offset;
    WebRtc_Word16 meanV, alpha;
    WebRtc_Word32 tmp32, tmp32_1;
    WebRtc_Word16 *valptr, *idxptr, *p1, *p2, *p3;

    // Offset to beginning of the 16 minimum values in memory
    offset = WEBRTC_SPL_LSHIFT_W16(n, 4);

    // Pointer to memory for the 16 minimum values and the age of each value
    idxptr = &inst->index_vector[offset];
    valptr = &inst->low_value_vector[offset];

    // Each value in low_value_vector is getting 1 loop older.
    // Update age of each value in indexVal, and remove old values.
    for (i = 0; i < 16; i++)
    {
        p3 = idxptr + i;
        if (*p3 != 100)
        {
            *p3 += 1;
        } else
        {
            p1 = valptr + i + 1;
            p2 = p3 + 1;
            for (j = i; j < 16; j++)
            {
                *(valptr + j) = *p1++;
                *(idxptr + j) = *p2++;
            }
            *(idxptr + 15) = 101;
            *(valptr + 15) = 10000;
        }
    }

    // Check if x smaller than any of the values in low_value_vector.
    // If so, find position.
    if (x < *(valptr + 7))
    {
        if (x < *(valptr + 3))
        {
            if (x < *(valptr + 1))
            {
                if (x < *valptr)
                {
                    II = 0;
                } else
                {
                    II = 1;
                }
            } else if (x < *(valptr + 2))
            {
                II = 2;
            } else
            {
                II = 3;
            }
        } else if (x < *(valptr + 5))
        {
            if (x < *(valptr + 4))
            {
                II = 4;
            } else
            {
                II = 5;
            }
        } else if (x < *(valptr + 6))
        {
            II = 6;
        } else
        {
            II = 7;
        }
    } else if (x < *(valptr + 15))
    {
        if (x < *(valptr + 11))
        {
            if (x < *(valptr + 9))
            {
                if (x < *(valptr + 8))
                {
                    II = 8;
                } else
                {
                    II = 9;
                }
            } else if (x < *(valptr + 10))
            {
                II = 10;
            } else
            {
                II = 11;
            }
        } else if (x < *(valptr + 13))
        {
            if (x < *(valptr + 12))
            {
                II = 12;
            } else
            {
                II = 13;
            }
        } else if (x < *(valptr + 14))
        {
            II = 14;
        } else
        {
            II = 15;
        }
    }

    // Put new min value on right position and shift bigger values up
    if (II > -1)
    {
        for (i = 15; i > II; i--)
        {
            k = i - 1;
            *(valptr + i) = *(valptr + k);
            *(idxptr + i) = *(idxptr + k);
        }
        *(valptr + II) = x;
        *(idxptr + II) = 1;
    }

    meanV = 0;
    if ((inst->frame_counter) > 4)
    {
        j = 5;
    } else
    {
        j = inst->frame_counter;
    }

    if (j > 2)
    {
        meanV = *(valptr + 2);
    } else if (j > 0)
    {
        meanV = *valptr;
    } else
    {
        meanV = 1600;
    }

    if (inst->frame_counter > 0)
    {
        if (meanV < inst->mean_value[n])
        {
            alpha = (WebRtc_Word16)ALPHA1; // 0.2 in Q15
        } else
        {
            alpha = (WebRtc_Word16)ALPHA2; // 0.99 in Q15
        }
    } else
    {
        alpha = 0;
    }

    tmp32 = WEBRTC_SPL_MUL_16_16((alpha+1), inst->mean_value[n]);
    tmp32_1 = WEBRTC_SPL_MUL_16_16(WEBRTC_SPL_WORD16_MAX - alpha, meanV);
    tmp32 += tmp32_1;
    tmp32 += 16384;
    inst->mean_value[n] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32, 15);

    return inst->mean_value[n];
}
