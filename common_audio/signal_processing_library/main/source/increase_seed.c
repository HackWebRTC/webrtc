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
 * This file contains the function WebRtcSpl_IncreaseSeed().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_UWord32 WebRtcSpl_IncreaseSeed(WebRtc_UWord32 *seed)
{
    seed[0] = (seed[0] * ((WebRtc_Word32)69069) + 1) & (WEBRTC_SPL_MAX_SEED_USED - 1);
    return seed[0];
}
