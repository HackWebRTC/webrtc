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
 * This file includes the constant values used internally in VAD.
 */

#include "vad_const.h"

// Spectrum Weighting
const WebRtc_Word16 kSpectrumWeight[6] = {6, 8, 10, 12, 14, 16};

const WebRtc_Word16 kCompVar = 22005;

// Constant 160*log10(2) in Q9
const WebRtc_Word16 kLogConst = 24660;

// Constant log2(exp(1)) in Q12
const WebRtc_Word16 kLog10Const = 5909;

// Q15
const WebRtc_Word16 kNoiseUpdateConst = 655;
const WebRtc_Word16 kSpeechUpdateConst = 6554;

// Q8
const WebRtc_Word16 kBackEta = 154;

// Coefficients used by WebRtcVad_HpOutput, Q14
const WebRtc_Word16 kHpZeroCoefs[3] = {6631, -13262, 6631};
const WebRtc_Word16 kHpPoleCoefs[3] = {16384, -7756, 5620};

// Allpass filter coefficients, upper and lower, in Q15
// Upper: 0.64, Lower: 0.17
const WebRtc_Word16 kAllPassCoefsQ15[2] = {20972, 5571};
const WebRtc_Word16 kAllPassCoefsQ13[2] = {5243, 1392}; // Q13

// Minimum difference between the two models, Q5
const WebRtc_Word16 kMinimumDifference[6] = {544, 544, 576, 576, 576, 576};

// Upper limit of mean value for speech model, Q7
const WebRtc_Word16 kMaximumSpeech[6] = {11392, 11392, 11520, 11520, 11520, 11520};

// Minimum value for mean value
const WebRtc_Word16 kMinimumMean[2] = {640, 768};

// Upper limit of mean value for noise model, Q7
const WebRtc_Word16 kMaximumNoise[6] = {9216, 9088, 8960, 8832, 8704, 8576};

// Adjustment for division with two in WebRtcVad_SplitFilter
const WebRtc_Word16 kOffsetVector[6] = {368, 368, 272, 176, 176, 176};

// Start values for the Gaussian models, Q7
// Weights for the two Gaussians for the six channels (noise)
const WebRtc_Word16 kNoiseDataWeights[12] = {34, 62, 72, 66, 53, 25, 94, 66, 56, 62, 75, 103};

// Weights for the two Gaussians for the six channels (speech)
const WebRtc_Word16 kSpeechDataWeights[12] = {48, 82, 45, 87, 50, 47, 80, 46, 83, 41, 78, 81};

// Means for the two Gaussians for the six channels (noise)
const WebRtc_Word16 kNoiseDataMeans[12] = {6738, 4892, 7065, 6715, 6771, 3369, 7646, 3863,
        7820, 7266, 5020, 4362};

// Means for the two Gaussians for the six channels (speech)
const WebRtc_Word16 kSpeechDataMeans[12] = {8306, 10085, 10078, 11823, 11843, 6309, 9473,
        9571, 10879, 7581, 8180, 7483};

// Stds for the two Gaussians for the six channels (noise)
const WebRtc_Word16 kNoiseDataStds[12] = {378, 1064, 493, 582, 688, 593, 474, 697, 475, 688,
        421, 455};

// Stds for the two Gaussians for the six channels (speech)
const WebRtc_Word16 kSpeechDataStds[12] = {555, 505, 567, 524, 585, 1231, 509, 828, 492, 1540,
        1079, 850};
