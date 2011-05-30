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
 * This file contains the function WebRtcSpl_AddSatW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef SPL_NO_DOUBLE_IMPLEMENTATIONS

WebRtc_Word16 WebRtcSpl_AddSatW16(WebRtc_Word16 var1, WebRtc_Word16 var2)
{
    WebRtc_Word32 s_sum = (WebRtc_Word32)var1 + (WebRtc_Word32)var2;

    if (s_sum > WEBRTC_SPL_WORD16_MAX)
        s_sum = WEBRTC_SPL_WORD16_MAX;
    else if (s_sum < WEBRTC_SPL_WORD16_MIN)
        s_sum = WEBRTC_SPL_WORD16_MIN;

    return (WebRtc_Word16)s_sum;
}

#endif
