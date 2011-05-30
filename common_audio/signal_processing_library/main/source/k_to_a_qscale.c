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
 * This file contains the function WebRtcSpl_KToAQScale().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_KToAQScale(WebRtc_Word16* k, int use_order, int Q, WebRtc_Word16* a)
{
    WebRtc_Word16 any[WEBRTC_SPL_MAX_LPC_ORDER];
    WebRtc_Word16* aptr;
    WebRtc_Word16* aptr2;
    WebRtc_Word16* anyptr;
    WebRtc_Word16* kptr;
    int m, i, Qscale;

    Qscale = 15 - Q; // Q-domain for A-coeff
    kptr = k;
    *a = *k >> Qscale;

    for (m = 0; m < (use_order - 1); m++)
    {
        kptr++;
        aptr = a;
        aptr2 = &a[m];
        anyptr = any;

        for (i = 0; i < m + 1; i++)
            *anyptr++ = (*aptr++) + (WebRtc_Word16)(((WebRtc_Word32)(*aptr2--)
                    * (WebRtc_Word32)*kptr) >> 15);

        any[m + 1] = *kptr >> Qscale; // compute the next coefficient for next loop
        aptr = a;
        anyptr = any;
        for (i = 0; i < (m + 2); i++)
        {
            *aptr = *anyptr;
            *aptr++;
            *anyptr++;
        }
    }
}
