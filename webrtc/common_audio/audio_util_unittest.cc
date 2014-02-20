/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/typedefs.h"

namespace webrtc {

void ExpectArraysEq(const int16_t* ref, const int16_t* test, int length) {
  for (int i = 0; i < length; ++i) {
    EXPECT_EQ(test[i], ref[i]);
  }
}

TEST(AudioUtilTest, Clamp) {
  EXPECT_EQ(1000.f, ClampInt16(1000.f));
  EXPECT_EQ(32767.f, ClampInt16(32767.5f));
  EXPECT_EQ(-32768.f, ClampInt16(-32768.5f));
}

TEST(AudioUtilTest, Round) {
  const int kSize = 7;
  const float kInput[kSize] = {
      0.f, 0.4f, 0.5f, -0.4f, -0.5f, 32768.f, -32769.f};
  const int16_t kReference[kSize] = {0, 0, 1, 0, -1, 32767, -32768};
  int16_t output[kSize];
  RoundToInt16(kInput, kSize, output);
  for (int n = 0; n < kSize; ++n)
    EXPECT_EQ(kReference[n], output[n]);
}

TEST(AudioUtilTest, InterleavingStereo) {
  const int16_t kInterleaved[] = {2, 3, 4, 9, 8, 27, 16, 81};
  const int kSamplesPerChannel = 4;
  const int kNumChannels = 2;
  const int kLength = kSamplesPerChannel * kNumChannels;
  int16_t left[kSamplesPerChannel], right[kSamplesPerChannel];
  int16_t* deinterleaved[] = {left, right};
  Deinterleave(kInterleaved, kSamplesPerChannel, kNumChannels, deinterleaved);
  const int16_t kRefLeft[] = {2, 4, 8, 16};
  const int16_t kRefRight[] = {3, 9, 27, 81};
  ExpectArraysEq(left, kRefLeft, kSamplesPerChannel);
  ExpectArraysEq(right, kRefRight, kSamplesPerChannel);

  int16_t interleaved[kLength];
  Interleave(deinterleaved, kSamplesPerChannel, kNumChannels, interleaved);
  ExpectArraysEq(interleaved, kInterleaved, kLength);
}

TEST(AudioUtilTest, InterleavingMonoIsIdentical) {
  const int16_t kInterleaved[] = {1, 2, 3, 4, 5};
  const int kSamplesPerChannel = 5;
  const int kNumChannels = 1;
  int16_t mono[kSamplesPerChannel];
  int16_t* deinterleaved[] = {mono};
  Deinterleave(kInterleaved, kSamplesPerChannel, kNumChannels, deinterleaved);
  ExpectArraysEq(mono, kInterleaved, kSamplesPerChannel);

  int16_t interleaved[kSamplesPerChannel];
  Interleave(deinterleaved, kSamplesPerChannel, kNumChannels, interleaved);
  ExpectArraysEq(interleaved, mono, kSamplesPerChannel);
}

}  // namespace webrtc
