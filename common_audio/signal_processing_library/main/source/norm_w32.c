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
 * This file contains the function WebRtcSpl_NormW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef SPL_NO_DOUBLE_IMPLEMENTATIONS

int WebRtcSpl_NormW32(WebRtc_Word32 value)
{
    int zeros = 0;

    if (value <= 0)
        value ^= 0xFFFFFFFF;

    // Fast binary search to determine the number of left shifts required to 32-bit normalize
    // the value
    if (!(0xFFFF8000 & value))
        zeros = 16;
    if (!(0xFF800000 & (value << zeros)))
        zeros += 8;
    if (!(0xF8000000 & (value << zeros)))
        zeros += 4;
    if (!(0xE0000000 & (value << zeros)))
        zeros += 2;
    if (!(0xC0000000 & (value << zeros)))
        zeros += 1;

    return zeros;
}

#endif
