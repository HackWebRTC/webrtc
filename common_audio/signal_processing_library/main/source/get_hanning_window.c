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
 * This file contains the function WebRtcSpl_GetHanningWindow().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_GetHanningWindow(WebRtc_Word16 *v, WebRtc_Word16 size)
{
    int jj;
    WebRtc_Word16 *vptr1;

    WebRtc_Word32 index;
    WebRtc_Word32 factor = ((WebRtc_Word32)0x40000000);

    factor = WebRtcSpl_DivW32W16(factor, size);
    if (size < 513)
        index = (WebRtc_Word32)-0x200000;
    else
        index = (WebRtc_Word32)-0x100000;
    vptr1 = v;

    for (jj = 0; jj < size; jj++)
    {
        index += factor;
        (*vptr1++) = WebRtcSpl_kHanningTable[index >> 22];
    }

}
