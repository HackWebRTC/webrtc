/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq4/tools/neteq_performance_test.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/typedefs.h"

TEST(NetEqPerformanceTest, Run) {
  const int kSimulationTimeMs = 10000000;
  const int kLossPeriod = 10;  // Drop every 10th packet.
  const double kDriftFactor = 0.1;
  int64_t runtime = webrtc::test::NetEqPerformanceTest::Run(
      kSimulationTimeMs, kLossPeriod, kDriftFactor);
  ASSERT_GT(runtime, 0);
  webrtc::test::PrintResult(
      "neteq4-runtime", "", "", runtime, "ms", true);
}
