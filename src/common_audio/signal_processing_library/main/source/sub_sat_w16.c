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
 * This file contains the function WebRtcSpl_SubSatW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef XSCALE_OPT
WebRtc_Word16 WebRtcSpl_SubSatW16(WebRtc_Word16 var1, WebRtc_Word16 var2)
{
    WebRtc_Word32 l_diff;
    WebRtc_Word16 s_diff;

    // perform subtraction
    l_diff = (WebRtc_Word32)var1 - (WebRtc_Word32)var2;

    // default setting
    s_diff = (WebRtc_Word16)l_diff;

    // check for overflow
    if (l_diff > (WebRtc_Word32)32767)
        s_diff = (WebRtc_Word16)32767;

    // check for underflow
    if (l_diff < (WebRtc_Word32)-32768)
        s_diff = (WebRtc_Word16)-32768;

    return s_diff;
}
#else
#pragma message(">> WebRtcSpl_SubSatW16.c is excluded from this build")
#endif // XSCALE_OPT
