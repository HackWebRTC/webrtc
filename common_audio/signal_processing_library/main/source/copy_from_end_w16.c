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
 * This file contains the function WebRtcSpl_CopyFromEndW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromEndW16(G_CONST WebRtc_Word16 *vector_in,
                                       WebRtc_Word16 length,
                                       WebRtc_Word16 samples,
                                       WebRtc_Word16 *vector_out,
                                       WebRtc_Word16 max_length)
{
    // Unused input variable
    max_length = max_length;

    // Copy the last <samples> of the input vector to vector_out
    WEBRTC_SPL_MEMCPY_W16(vector_out, &vector_in[length - samples], samples);

    return samples;
}
