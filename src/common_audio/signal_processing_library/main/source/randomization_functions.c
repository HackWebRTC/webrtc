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
 * This file contains implementations of the randomization functions
 * WebRtcSpl_IncreaseSeed()
 * WebRtcSpl_RandU()
 * WebRtcSpl_RandN()
 * WebRtcSpl_RandUArray()
 *
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_UWord32 WebRtcSpl_IncreaseSeed(WebRtc_UWord32 *seed)
{
    seed[0] = (seed[0] * ((WebRtc_Word32)69069) + 1) & (WEBRTC_SPL_MAX_SEED_USED - 1);
    return seed[0];
}

WebRtc_Word16 WebRtcSpl_RandU(WebRtc_UWord32 *seed)
{
    return (WebRtc_Word16)(WebRtcSpl_IncreaseSeed(seed) >> 16);
}

WebRtc_Word16 WebRtcSpl_RandN(WebRtc_UWord32 *seed)
{
    return WebRtcSpl_kRandNTable[WebRtcSpl_IncreaseSeed(seed) >> 23];
}

// Creates an array of uniformly distributed variables
WebRtc_Word16 WebRtcSpl_RandUArray(WebRtc_Word16* vector,
                                   WebRtc_Word16 vector_length,
                                   WebRtc_UWord32* seed)
{
    int i;
    for (i = 0; i < vector_length; i++)
    {
        vector[i] = WebRtcSpl_RandU(seed);
    }
    return vector_length;
}
