/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/signal_processing/include/real_fft.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "typedefs.h"

#include "gtest/gtest.h"

namespace webrtc {
namespace {

const int kOrder = 3;
const int kLength = 1 << (kOrder + 1);  // +1 to hold complex data.
const int16_t kRefData[kLength] = {
    11739, -6848, -8688, 31980, -30295, 25242, 27085, 19410, -26299, -15607,
    -10791, 11778, -23819, 14498, -25772, 10076
};

class RealFFTTest : public ::testing::Test {
};

TEST_F(RealFFTTest, CreateFailsOnBadInput) {
  RealFFT* fft = WebRtcSpl_CreateRealFFT(11);
  EXPECT_TRUE(fft == NULL);
  fft = WebRtcSpl_CreateRealFFT(-1);
  EXPECT_TRUE(fft == NULL);
}

// TODO(andrew): Look more into why this was failing.
TEST_F(RealFFTTest, DISABLED_TransformIsInvertible) {
  int16_t data[kLength] = {0};
  memcpy(data, kRefData, sizeof(kRefData));

  RealFFT* fft = NULL;
  fft = WebRtcSpl_CreateRealFFT(kOrder);
  EXPECT_TRUE(fft != NULL);

  EXPECT_EQ(0, WebRtcSpl_RealForwardFFT(fft, data));
  int scale = WebRtcSpl_RealInverseFFT(fft, data);

  EXPECT_GE(scale, 0);
  for (int i = 0; i < kLength; i++) {
    EXPECT_EQ(data[i] << scale, kRefData[i]);
  }
  WebRtcSpl_FreeRealFFT(fft);
}

// TODO(andrew): This won't always be the case, but verifies the current code
// at least.
TEST_F(RealFFTTest, RealAndComplexAreIdentical) {
  int16_t real_data[kLength] = {0};
  int16_t complex_data[kLength] = {0};
  memcpy(real_data, kRefData, sizeof(kRefData));
  memcpy(complex_data, kRefData, sizeof(kRefData));

  RealFFT* fft = NULL;
  fft = WebRtcSpl_CreateRealFFT(kOrder);
  EXPECT_TRUE(fft != NULL);

  EXPECT_EQ(0, WebRtcSpl_RealForwardFFT(fft, real_data));
  EXPECT_EQ(0, WebRtcSpl_ComplexFFT(complex_data, kOrder, 1));
  for (int i = 0; i < kLength; i++) {
    EXPECT_EQ(real_data[i], complex_data[i]);
  }

  int real_scale =  WebRtcSpl_RealInverseFFT(fft, real_data);
  int complex_scale = WebRtcSpl_ComplexIFFT(complex_data, kOrder, 1);
  EXPECT_GE(real_scale, 0);
  EXPECT_EQ(real_scale, complex_scale);
  for (int i = 0; i < kLength; i++) {
    EXPECT_EQ(real_data[i], complex_data[i]);
  }
  WebRtcSpl_FreeRealFFT(fft);
}

}  // namespace
}  // namespace webrtc
