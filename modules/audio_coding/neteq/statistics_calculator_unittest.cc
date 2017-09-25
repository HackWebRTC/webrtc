/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/statistics_calculator.h"

#include "test/gtest.h"

namespace webrtc {

TEST(LifetimeStatistics, TotalSamplesReceived) {
  StatisticsCalculator stats;
  for (int i = 0; i < 10; ++i) {
    stats.IncreaseCounter(480, 48000);  // 10 ms at 48 kHz.
  }
  EXPECT_EQ(10 * 480u, stats.GetLifetimeStatistics().total_samples_received);
}

TEST(LifetimeStatistics, SamplesConcealed) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  stats.ExpandedNoiseSamples(17, false);
  EXPECT_EQ(100u + 17u, stats.GetLifetimeStatistics().concealed_samples);
}

// This test verifies that a negative correction of concealed_samples does not
// result in a decrease in the stats value (because stats-consuming applications
// would not expect the value to decrease). Instead, the correction should be
// made to future increments to the stat.
TEST(LifetimeStatistics, SamplesConcealedCorrection) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(-10);
  // Do not subtract directly, but keep the correction for later.
  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(20);
  // The total correction is 20 - 10.
  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);

  // Also test correction done to the next ExpandedVoiceSamples call.
  stats.ExpandedVoiceSamplesCorrection(-17);
  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(110u + 100u - 17u, stats.GetLifetimeStatistics().concealed_samples);
}

// This test verifies that neither "accelerate" nor "pre-emptive expand" reults
// in a modification to concealed_samples stats. Only PLC operations (i.e.,
// "expand" and "merge") should affect the stat.
TEST(LifetimeStatistics, NoUpdateOnTimeStretch) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  stats.AcceleratedSamples(4711);
  stats.PreemptiveExpandedSamples(17);
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(200u, stats.GetLifetimeStatistics().concealed_samples);
}

}  // namespace webrtc
