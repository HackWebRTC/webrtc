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
 * This file contains the function WebRtcSpl_SubSatW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef SPL_NO_DOUBLE_IMPLEMENTATIONS

WebRtc_Word32 WebRtcSpl_SubSatW32(WebRtc_Word32 var1, WebRtc_Word32 var2)
{
    WebRtc_Word32 l_diff;

    // perform subtraction
    l_diff = var1 - var2;

    // check for underflow
    if ((var1 < 0) && (var2 > 0) && (l_diff > 0))
        l_diff = (WebRtc_Word32)0x80000000;
    // check for overflow
    if ((var1 > 0) && (var2 < 0) && (l_diff < 0))
        l_diff = (WebRtc_Word32)0x7FFFFFFF;

    return l_diff;
}

#endif
