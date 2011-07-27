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
 * This file contains the function WebRtcSpl_GetSizeInBits().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_GetSizeInBits(WebRtc_UWord32 value)
{

    int bits = 0;

    // Fast binary search to find the number of bits used
    if ((0xFFFF0000 & value))
        bits = 16;
    if ((0x0000FF00 & (value >> bits)))
        bits += 8;
    if ((0x000000F0 & (value >> bits)))
        bits += 4;
    if ((0x0000000C & (value >> bits)))
        bits += 2;
    if ((0x00000002 & (value >> bits)))
        bits += 1;
    if ((0x00000001 & (value >> bits)))
        bits += 1;

    return bits;
}
