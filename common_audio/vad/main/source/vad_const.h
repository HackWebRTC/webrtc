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
 * This header file includes the declarations of the internally used constants.
 */

#ifndef WEBRTC_VAD_CONST_H_
#define WEBRTC_VAD_CONST_H_

#include "typedefs.h"

// TODO(ajm): give these internal-linkage by moving to the appropriate file
// where possible, and otherwise tag with WebRtcVad_.

// Spectrum Weighting
extern const WebRtc_Word16 kSpectrumWeight[];
extern const WebRtc_Word16 kCompVar;
// Logarithm constant
extern const WebRtc_Word16 kLogConst;
extern const WebRtc_Word16 kLog10Const;
// Q15
extern const WebRtc_Word16 kNoiseUpdateConst;
extern const WebRtc_Word16 kSpeechUpdateConst;
// Q8
extern const WebRtc_Word16 kBackEta;
// Coefficients used by WebRtcVad_HpOutput, Q14
extern const WebRtc_Word16 kHpZeroCoefs[];
extern const WebRtc_Word16 kHpPoleCoefs[];
// Allpass filter coefficients, upper and lower, in Q15 resp. Q13
extern const WebRtc_Word16 kAllPassCoefsQ15[];
extern const WebRtc_Word16 kAllPassCoefsQ13[];
// Minimum difference between the two models, Q5
extern const WebRtc_Word16 kMinimumDifference[];
// Maximum value when updating the speech model, Q7
extern const WebRtc_Word16 kMaximumSpeech[];
// Minimum value for mean value
extern const WebRtc_Word16 kMinimumMean[];
// Upper limit of mean value for noise model, Q7
extern const WebRtc_Word16 kMaximumNoise[];
// Adjustment for division with two in WebRtcVad_SplitFilter
extern const WebRtc_Word16 kOffsetVector[];
// Start values for the Gaussian models, Q7
extern const WebRtc_Word16 kNoiseDataWeights[];
extern const WebRtc_Word16 kSpeechDataWeights[];
extern const WebRtc_Word16 kNoiseDataMeans[];
extern const WebRtc_Word16 kSpeechDataMeans[];
extern const WebRtc_Word16 kNoiseDataStds[];
extern const WebRtc_Word16 kSpeechDataStds[];

#endif // WEBRTC_VAD_CONST_H_
