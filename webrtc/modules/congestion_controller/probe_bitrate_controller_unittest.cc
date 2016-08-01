/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"

#include <vector>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"

namespace webrtc {

class TestProbeBitrateEstimator : public ::testing::Test {
 public:
  TestProbeBitrateEstimator() : probe_bitrate_estimator_() {}

  void AddPacketFeedback(int probe_cluster_id,
                         size_t size,
                         int64_t send_time_ms,
                         int64_t arrival_time_ms) {
    PacketInfo info(arrival_time_ms, send_time_ms, 0, size, probe_cluster_id);
    ProbingResult res = probe_bitrate_estimator_.PacketFeedback(info);
    if (res.bps != ProbingResult::kNoEstimate)
      results_.emplace_back(res.bps, res.timestamp);
  }

  void CheckResult(size_t index, int bps, int max_diff, int64_t timestamp) {
    ASSERT_GT(results_.size(), index);
    EXPECT_NEAR(results_[index].first, bps, max_diff);
    EXPECT_EQ(results_[index].second, timestamp);
  }

 protected:
  std::vector<std::pair<int, int64_t>> results_;
  ProbeBitrateEstimator probe_bitrate_estimator_;
};

TEST_F(TestProbeBitrateEstimator, OneCluster) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 40, 50);

  CheckResult(0, 100000, 10, 50);
}

TEST_F(TestProbeBitrateEstimator, FastReceive) {
  AddPacketFeedback(0, 1000, 0, 15);
  AddPacketFeedback(0, 1000, 10, 30);
  AddPacketFeedback(0, 1000, 20, 40);
  AddPacketFeedback(0, 1000, 40, 50);

  CheckResult(0, 100000, 10, 50);
}

TEST_F(TestProbeBitrateEstimator, TooFastReceive) {
  AddPacketFeedback(0, 1000, 0, 19);
  AddPacketFeedback(0, 1000, 10, 30);
  AddPacketFeedback(0, 1000, 20, 40);
  AddPacketFeedback(0, 1000, 40, 50);

  EXPECT_TRUE(results_.empty());
}

TEST_F(TestProbeBitrateEstimator, SlowReceive) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 40);
  AddPacketFeedback(0, 1000, 20, 70);
  AddPacketFeedback(0, 1000, 40, 110);

  CheckResult(0, 40000, 10, 110);
}

TEST_F(TestProbeBitrateEstimator, BurstReceive) {
  AddPacketFeedback(0, 1000, 0, 50);
  AddPacketFeedback(0, 1000, 10, 50);
  AddPacketFeedback(0, 1000, 20, 50);
  AddPacketFeedback(0, 1000, 40, 50);

  EXPECT_TRUE(results_.empty());
}

TEST_F(TestProbeBitrateEstimator, MultipleClusters) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 40, 60);
  AddPacketFeedback(0, 1000, 50, 60);

  CheckResult(0, 80000, 10, 60);
  CheckResult(1, 100000, 10, 60);

  AddPacketFeedback(1, 1000, 60, 70);
  AddPacketFeedback(1, 1000, 65, 77);
  AddPacketFeedback(1, 1000, 70, 84);
  AddPacketFeedback(1, 1000, 75, 90);

  CheckResult(2, 200000, 10, 90);
}

TEST_F(TestProbeBitrateEstimator, OldProbe) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);

  AddPacketFeedback(1, 1000, 60, 70);
  AddPacketFeedback(1, 1000, 65, 77);
  AddPacketFeedback(1, 1000, 70, 84);
  AddPacketFeedback(1, 1000, 75, 90);

  CheckResult(0, 200000, 10, 90);

  AddPacketFeedback(0, 1000, 40, 60);

  EXPECT_EQ(1ul, results_.size());
}

}  // namespace webrtc
