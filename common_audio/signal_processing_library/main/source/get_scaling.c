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
 * This file contains the function WebRtcSpl_GetScaling().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_GetScaling(WebRtc_Word16 *in_vector, int in_vector_length, int times)
{
    int nbits = WebRtcSpl_GetSizeInBits(times);
    int i;
    WebRtc_Word16 sabs;
    int t;
    WebRtc_Word16 *sptr = in_vector;
    WebRtc_Word16 smax = *sptr++;

    for (i = in_vector_length - 1; i > 0; i--)
    {
        sabs = WEBRTC_SPL_ABS_W16 (*sptr);
        sptr++;
        if (sabs > smax)
            smax = sabs;
    }

    t = WebRtcSpl_NormW32((WebRtc_Word32)smax << 16);

    if (smax == 0)
    {
        return 0; // Since norm(0) returns 0
    } else
    {
        return (t > nbits) ? 0 : nbits - t;
    }
}
