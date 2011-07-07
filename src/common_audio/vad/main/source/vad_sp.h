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
 * This header file includes the VAD internal calls for Downsampling and FindMinimum.
 * Specific function calls are given below.
 */

#ifndef WEBRTC_VAD_SP_H_
#define WEBRTC_VAD_SP_H_

#include "vad_core.h"

/****************************************************************************
 * WebRtcVad_Downsampling(...)
 *
 * Downsamples the signal a factor 2, eg. 32->16 or 16->8
 *
 * Input:
 *      - signal_in     : Input signal
 *      - in_length     : Length of input signal in samples
 *
 * Input & Output:
 *      - filter_state  : Filter state for first all-pass filters
 *
 * Output:
 *      - signal_out    : Downsampled signal (of length len/2)
 */
void WebRtcVad_Downsampling(WebRtc_Word16* signal_in,
                            WebRtc_Word16* signal_out,
                            WebRtc_Word32* filter_state,
                            int in_length);

/****************************************************************************
 * WebRtcVad_FindMinimum(...)
 *
 * Find the five lowest values of x in 100 frames long window. Return a mean
 * value of these five values.
 *
 * Input:
 *      - feature_value : Feature value
 *      - channel       : Channel number
 *
 * Input & Output:
 *      - inst          : State information
 *
 * Output:
 *      return value    : Weighted minimum value for a moving window.
 */
WebRtc_Word16 WebRtcVad_FindMinimum(VadInstT* inst, WebRtc_Word16 feature_value, int channel);

#endif // WEBRTC_VAD_SP_H_
