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
 * This file contains the function WebRtcSpl_NormW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_NormW16(WebRtc_Word16 value)
{
    int zeros = 0;

    if (value <= 0)
        value ^= 0xFFFF;

    if ( !(0xFF80 & value))
        zeros = 8;
    if ( !(0xF800 & (value << zeros)))
        zeros += 4;
    if ( !(0xE000 & (value << zeros)))
        zeros += 2;
    if ( !(0xC000 & (value << zeros)))
        zeros += 1;

    return zeros;
}
