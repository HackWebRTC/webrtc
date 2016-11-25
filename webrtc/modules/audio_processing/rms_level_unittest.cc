/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cmath>
#include <memory>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/mathutils.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/modules/audio_processing/rms_level.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
constexpr int kSampleRateHz = 48000;
constexpr size_t kBlockSizeSamples = kSampleRateHz / 100;

std::unique_ptr<RMSLevel> RunTest(rtc::ArrayView<const int16_t> input) {
  std::unique_ptr<RMSLevel> level(new RMSLevel);
  for (size_t n = 0; n + kBlockSizeSamples <= input.size();
       n += kBlockSizeSamples) {
    level->Process(input.subview(n, kBlockSizeSamples).data(),
                   kBlockSizeSamples);
  }
  return level;
}

std::vector<int16_t> CreateSinusoid(int frequency_hz,
                                    int amplitude,
                                    size_t num_samples) {
  std::vector<int16_t> x(num_samples);
  for (size_t n = 0; n < num_samples; ++n) {
    x[n] = rtc::saturated_cast<int16_t>(
        amplitude * std::sin(2 * M_PI * n * frequency_hz / kSampleRateHz));
  }
  return x;
}
}  // namespace

TEST(RmsLevelTest, Run1000HzFullScale) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(3, level->RMS());  // -3 dBFS
}

TEST(RmsLevelTest, Run1000HzHalfScale) {
  auto x = CreateSinusoid(1000, INT16_MAX / 2, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(9, level->RMS());  // -9 dBFS
}

TEST(RmsLevelTest, RunZeros) {
  std::vector<int16_t> x(kSampleRateHz, 0);  // 1 second of pure silence.
  auto level = RunTest(x);
  EXPECT_EQ(127, level->RMS());
}

TEST(RmsLevelTest, NoSamples) {
  RMSLevel level;
  EXPECT_EQ(127, level.RMS());  // Return minimum if no samples are given.
}

TEST(RmsLevelTest, PollTwice) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->RMS();
  EXPECT_EQ(127, level->RMS());  // Stats should be reset at this point.
}

TEST(RmsLevelTest, Reset) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->Reset();
  EXPECT_EQ(127, level->RMS());  // Stats should be reset at this point.
}

// Inserts 1 second of full-scale sinusoid, followed by 1 second of muted.
TEST(RmsLevelTest, ProcessMuted) {
  auto x = CreateSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->ProcessMuted(kSampleRateHz);
  EXPECT_EQ(6, level->RMS());  // Average RMS halved due to the silence.
}

}  // namespace webrtc
