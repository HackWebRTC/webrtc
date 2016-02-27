/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"

namespace webrtc {

namespace {

// Target output for ERB create test. Generated with matlab.
const float kTestCenterFreqs[] = {
    14.5213f, 29.735f,  45.6781f, 62.3884f, 79.9058f, 98.2691f, 117.521f,
    137.708f, 158.879f, 181.084f, 204.378f, 228.816f, 254.459f, 281.371f,
    309.618f, 339.273f, 370.411f, 403.115f, 437.469f, 473.564f, 511.497f,
    551.371f, 593.293f, 637.386f, 683.77f,  732.581f, 783.96f,  838.06f,
    895.046f, 955.09f,  1018.38f, 1085.13f, 1155.54f, 1229.85f, 1308.32f,
    1391.22f, 1478.83f, 1571.5f,  1669.55f, 1773.37f, 1883.37f, 2000.f};
const float kTestFilterBank[][33] = {
    {0.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.2f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.25f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,   0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.25f, 0.142857f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.25f, 0.285714f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,   0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.285714f, 0.142857f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.285714f, 0.285714f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.285714f, 0.142857f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.285714f, 0.285714f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.142857f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.285714f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.142857f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.285714f, 0.157895f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.210526f, 0.117647f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f, 0.315789f, 0.176471f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.315789f, 0.352941f, 0.142857f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f},
    {0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.352941f, 0.285714f,
     0.157895f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,
     0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f},
    {0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.285714f,
     0.210526f, 0.111111f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,       0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.285714f, 0.315789f, 0.222222f, 0.111111f, 0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,       0.f,       0.f,       0.f,       0.f},
    {0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f, 0.f,
     0.f, 0.f, 0.315789f, 0.333333f, 0.222222f, 0.111111f, 0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f, 0.f,
     0.f, 0.f, 0.f,       0.f,       0.f,       0.f},
    {0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f,
     0.f, 0.f, 0.f, 0.333333f, 0.333333f, 0.222222f, 0.111111f, 0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f, 0.f,
     0.f, 0.f, 0.f, 0.f,       0.f,       0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.333333f, 0.333333f, 0.222222f, 0.111111f, 0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f,       0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.333333f, 0.333333f, 0.222222f, 0.111111f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,
     0.f,       0.f, 0.f, 0.f, 0.f, 0.f, 0.333333f, 0.333333f, 0.222222f,
     0.108108f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,
     0.f,       0.f, 0.f, 0.f, 0.f, 0.f},
    {0.f,       0.f,       0.f,        0.f, 0.f, 0.f, 0.f, 0.f,       0.f,
     0.f,       0.f,       0.f,        0.f, 0.f, 0.f, 0.f, 0.333333f, 0.333333f,
     0.243243f, 0.153846f, 0.0833333f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,
     0.f,       0.f,       0.f,        0.f, 0.f, 0.f},
    {0.f,       0.f,       0.f,       0.f,        0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,       0.f,       0.f,       0.f,        0.f, 0.f, 0.f, 0.f, 0.333333f,
     0.324324f, 0.230769f, 0.166667f, 0.0909091f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,       0.f,       0.f,       0.f,        0.f, 0.f},
    {0.f,       0.f,       0.f,   0.f,       0.f,        0.f, 0.f, 0.f, 0.f,
     0.f,       0.f,       0.f,   0.f,       0.f,        0.f, 0.f, 0.f, 0.f,
     0.324324f, 0.307692f, 0.25f, 0.181818f, 0.0833333f, 0.f, 0.f, 0.f, 0.f,
     0.f,       0.f,       0.f,   0.f,       0.f,        0.f},
    {0.f,       0.f,   0.f,       0.f,        0.f, 0.f,       0.f,
     0.f,       0.f,   0.f,       0.f,        0.f, 0.f,       0.f,
     0.f,       0.f,   0.f,       0.f,        0.f, 0.307692f, 0.333333f,
     0.363636f, 0.25f, 0.151515f, 0.0793651f, 0.f, 0.f,       0.f,
     0.f,       0.f,   0.f,       0.f,        0.f},
    {0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,       0.f,       0.166667f,  0.363636f, 0.333333f, 0.242424f,
     0.190476f, 0.133333f, 0.0689655f, 0.f,       0.f,       0.f,
     0.f,       0.f,       0.f},
    {0.f,        0.f, 0.f, 0.f, 0.f,       0.f,      0.f,       0.f,  0.f,
     0.f,        0.f, 0.f, 0.f, 0.f,       0.f,      0.f,       0.f,  0.f,
     0.f,        0.f, 0.f, 0.f, 0.333333f, 0.30303f, 0.253968f, 0.2f, 0.137931f,
     0.0714286f, 0.f, 0.f, 0.f, 0.f,       0.f},
    {0.f,    0.f,        0.f,      0.f,      0.f,       0.f,       0.f,
     0.f,    0.f,        0.f,      0.f,      0.f,       0.f,       0.f,
     0.f,    0.f,        0.f,      0.f,      0.f,       0.f,       0.f,
     0.f,    0.f,        0.30303f, 0.31746f, 0.333333f, 0.275862f, 0.214286f,
     0.125f, 0.0655738f, 0.f,      0.f,      0.f},
    {0.f,   0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,   0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,   0.f,       0.f,       0.f,        0.f,       0.f,       0.f,
     0.f,   0.f,       0.f,       0.15873f,   0.333333f, 0.344828f, 0.357143f,
     0.25f, 0.196721f, 0.137931f, 0.0816327f, 0.f},
    {0.f,     0.f,       0.f,       0.f,       0.f, 0.f,       0.f,
     0.f,     0.f,       0.f,       0.f,       0.f, 0.f,       0.f,
     0.f,     0.f,       0.f,       0.f,       0.f, 0.f,       0.f,
     0.f,     0.f,       0.f,       0.f,       0.f, 0.172414f, 0.357143f,
     0.3125f, 0.245902f, 0.172414f, 0.102041f, 0.f},
    {0.f, 0.f,     0.f,       0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,     0.f,       0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.f,     0.f,       0.f,       0.f,       0.f, 0.f, 0.f, 0.f,
     0.f, 0.3125f, 0.327869f, 0.344828f, 0.204082f, 0.f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,       0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.163934f, 0.344828f, 0.408163f, 0.5f},
    {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,       0.f,
     0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.204082f, 0.5f}};
static_assert(arraysize(kTestCenterFreqs) == arraysize(kTestFilterBank),
              "Test filterbank badly initialized.");

// Target output for gain solving test. Generated with matlab.
const size_t kTestStartFreq = 12;  // Lowest integral frequency for ERBs.
const float kTestZeroVar = 1.f;
const float kTestNonZeroVarLambdaTop[] = {
    1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
static_assert(arraysize(kTestCenterFreqs) ==
                  arraysize(kTestNonZeroVarLambdaTop),
              "Power test data badly initialized.");
const float kMaxTestError = 0.005f;

// Enhancer initialization parameters.
const int kSamples = 1000;
const int kSampleRate = 4000;
const int kNumChannels = 1;
const int kFragmentSize = kSampleRate / 100;

}  // namespace

class IntelligibilityEnhancerTest : public ::testing::Test {
 protected:
  IntelligibilityEnhancerTest()
      : clear_data_(kSamples), noise_data_(kSamples), orig_data_(kSamples) {
    enh_.reset(new IntelligibilityEnhancer(kSampleRate, kNumChannels));
  }

  bool CheckUpdate() {
    enh_.reset(new IntelligibilityEnhancer(kSampleRate, kNumChannels));
    float* clear_cursor = clear_data_.data();
    float* noise_cursor = noise_data_.data();
    for (int i = 0; i < kSamples; i += kFragmentSize) {
      enh_->ProcessRenderAudio(&clear_cursor, kSampleRate, kNumChannels);
      clear_cursor += kFragmentSize;
      noise_cursor += kFragmentSize;
    }
    for (int i = 0; i < kSamples; i++) {
      if (std::fabs(clear_data_[i] - orig_data_[i]) > kMaxTestError) {
        return true;
      }
    }
    return false;
  }

  std::unique_ptr<IntelligibilityEnhancer> enh_;
  std::vector<float> clear_data_;
  std::vector<float> noise_data_;
  std::vector<float> orig_data_;
};

// For each class of generated data, tests that render stream is updated when
// it should be.
TEST_F(IntelligibilityEnhancerTest, TestRenderUpdate) {
  std::fill(noise_data_.begin(), noise_data_.end(), 0.f);
  std::fill(orig_data_.begin(), orig_data_.end(), 0.f);
  std::fill(clear_data_.begin(), clear_data_.end(), 0.f);
  EXPECT_FALSE(CheckUpdate());
  std::srand(1);
  auto float_rand = []() { return std::rand() * 2.f / RAND_MAX - 1; };
  std::generate(noise_data_.begin(), noise_data_.end(), float_rand);
  EXPECT_FALSE(CheckUpdate());
  std::generate(clear_data_.begin(), clear_data_.end(), float_rand);
  orig_data_ = clear_data_;
  EXPECT_TRUE(CheckUpdate());
}

// Tests ERB bank creation, comparing against matlab output.
TEST_F(IntelligibilityEnhancerTest, TestErbCreation) {
  ASSERT_EQ(arraysize(kTestCenterFreqs), enh_->bank_size_);
  for (size_t i = 0; i < enh_->bank_size_; ++i) {
    EXPECT_NEAR(kTestCenterFreqs[i], enh_->center_freqs_[i], kMaxTestError);
    ASSERT_EQ(arraysize(kTestFilterBank[0]), enh_->freqs_);
    for (size_t j = 0; j < enh_->freqs_; ++j) {
      EXPECT_NEAR(kTestFilterBank[i][j], enh_->render_filter_bank_[i][j],
                  kMaxTestError);
    }
  }
}

// Tests analytic solution for optimal gains, comparing
// against matlab output.
TEST_F(IntelligibilityEnhancerTest, TestSolveForGains) {
  ASSERT_EQ(kTestStartFreq, enh_->start_freq_);
  std::vector<float> sols(enh_->bank_size_);
  float lambda = -0.001f;
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    enh_->filtered_clear_pow_[i] = 0.f;
    enh_->filtered_noise_pow_[i] = 0.f;
  }
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, sols.data());
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestZeroVar, sols[i], kMaxTestError);
  }
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    enh_->filtered_clear_pow_[i] = static_cast<float>(i + 1);
    enh_->filtered_noise_pow_[i] = static_cast<float>(enh_->bank_size_ - i);
  }
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, sols.data());
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestNonZeroVarLambdaTop[i], sols[i], kMaxTestError);
  }
  lambda = -1.f;
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, sols.data());
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestNonZeroVarLambdaTop[i], sols[i], kMaxTestError);
  }
}

}  // namespace webrtc
