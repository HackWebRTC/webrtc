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
 * This header file includes the description of the internal VAD call
 * WebRtcVad_GaussianProbability.
 */

#ifndef WEBRTC_VAD_GMM_H_
#define WEBRTC_VAD_GMM_H_

#include "typedefs.h"

/****************************************************************************
 * WebRtcVad_GaussianProbability(...)
 *
 * This function calculates the probability for the value 'in_sample', given that in_sample
 * comes from a normal distribution with mean 'mean' and standard deviation 'std'.
 *
 * Input:
 *      - in_sample     : Input sample in Q4
 *      - mean          : mean value in the statistical model, Q7
 *      - std           : standard deviation, Q7
 *
 * Output:
 *
 *      - delta         : Value used when updating the model, Q11
 *
 * Return:
 *      - out           : out = 1/std * exp(-(x-m)^2/(2*std^2));
 *                        Probability for x.
 *
 */
WebRtc_Word32 WebRtcVad_GaussianProbability(WebRtc_Word16 in_sample,
                                            WebRtc_Word16 mean,
                                            WebRtc_Word16 std,
                                            WebRtc_Word16 *delta);

#endif // WEBRTC_VAD_GMM_H_
