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
 * This file contains the function WebRtcSpl_Lpc().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_Lpc(G_CONST WebRtc_Word16 *x, int x_length, int order,
                              WebRtc_Word16 *lpcvec) // out Q12
{
    int cvlen, corrvlen;
    int scaleDUMMY;
    WebRtc_Word32 corrvector[WEBRTC_SPL_MAX_LPC_ORDER + 1];
    WebRtc_Word16 reflCoefs[WEBRTC_SPL_MAX_LPC_ORDER];

    cvlen = order + 1;
    corrvlen = WebRtcSpl_AutoCorrelation(x, x_length, order, corrvector, &scaleDUMMY);
    if (*corrvector == 0)
        *corrvector = WEBRTC_SPL_WORD16_MAX;

    WebRtcSpl_AutoCorrToReflCoef(corrvector, order, reflCoefs);
    WebRtcSpl_ReflCoefToLpc(reflCoefs, order, lpcvec);

    return cvlen;
}
