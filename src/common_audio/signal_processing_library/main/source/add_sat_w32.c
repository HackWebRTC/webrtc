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
 * This file contains the function WebRtcSpl_AddSatW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef SPL_NO_DOUBLE_IMPLEMENTATIONS

WebRtc_Word32 WebRtcSpl_AddSatW32(WebRtc_Word32 var1, WebRtc_Word32 var2)
{
    WebRtc_Word32 l_sum;

    // perform long addition
    l_sum = var1 + var2;

    // check for under or overflow
    if (WEBRTC_SPL_IS_NEG(var1))
    {
        if (WEBRTC_SPL_IS_NEG(var2) && !WEBRTC_SPL_IS_NEG(l_sum))
        {
            l_sum = (WebRtc_Word32)0x80000000;
        }
    } else
    {
        if (!WEBRTC_SPL_IS_NEG(var2) && WEBRTC_SPL_IS_NEG(l_sum))
        {
            l_sum = (WebRtc_Word32)0x7FFFFFFF;
        }
    }

    return l_sum;
}

#endif
