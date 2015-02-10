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
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweSimulation : public BweTest,
                      public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
  BweSimulation() : BweTest() {}
  virtual ~BweSimulation() {}

 protected:
  virtual void SetUp() OVERRIDE { BweTest::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        BweSimulation,
                        ::testing::Values(kRembEstimator,
                                          kFullSendSideEstimator));

TEST_P(BweSimulation, SprintUplinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(this, &source, GetParam());
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("sprint-uplink", "rx")));
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Verizon4gDownlinkTest) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(this, &source, GetParam());
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  RateCounterFilter counter2(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbps) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(this, &source, GetParam());
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  filter.SetCapacity(1000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(500);
  RunFor(60 * 1000);
  filter.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke1000kbps500kbps1000kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, &source, GetParam());
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  filter.SetCapacity(1000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(500);
  RunFor(60 * 1000);
  filter.SetCapacity(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke10000kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, &source, GetParam());
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  filter.SetCapacity(10000);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke200kbps30kbps200kbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, &source, GetParam());
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  filter.SetCapacity(200);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(30);
  RunFor(60 * 1000);
  filter.SetCapacity(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke200kbps30kbps200kbps) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(this, &source, GetParam());
  ChokeFilter filter(this);
  RateCounterFilter counter(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  filter.SetCapacity(200);
  filter.SetMaxDelay(500);
  RunFor(60 * 1000);
  filter.SetCapacity(30);
  RunFor(60 * 1000);
  filter.SetCapacity(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, GoogleWifiTrace3Mbps) {
  VerboseLogging(true);
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacketSender sender(this, &source, kRembEstimator);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, PacerGoogleWifiTrace3Mbps) {
  VerboseLogging(true);
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(this, &source, kRembEstimator);
  RateCounterFilter counter1(this, "sender_output");
  TraceBasedDeliveryFilter filter(this, "link_capacity");
  filter.SetMaxDelay(500);
  RateCounterFilter counter2(this, "receiver_input");
  PacketReceiver receiver(this, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, SelfFairnessTest) {
  VerboseLogging(true);
  const int kAllFlowIds[] = {0, 1, 2};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  scoped_ptr<AdaptiveVideoSource> sources[kNumFlows];
  scoped_ptr<PacketSender> senders[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    sources[i].reset(
        new AdaptiveVideoSource(kAllFlowIds[i], 30, 300, 0, i * 20000));
    senders[i].reset(new PacketSender(this, sources[i].get(), GetParam()));
  }

  ChokeFilter choke(this, CreateFlowIds(kAllFlowIds, kNumFlows));
  choke.SetCapacity(1000);

  scoped_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(new RateCounterFilter(
        this, CreateFlowIds(&kAllFlowIds[i], 1), "receiver_input"));
  }

  RateCounterFilter total_utilization(
      this, CreateFlowIds(kAllFlowIds, kNumFlows), "total_utilization");

  scoped_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    receivers[i].reset(
        new PacketReceiver(this, kAllFlowIds[i], GetParam(), i == 0, false));
  }

  RunFor(30 * 60 * 1000);
}

TEST_P(BweSimulation, PacedSelfFairnessTest) {
  VerboseLogging(true);
  const int kAllFlowIds[] = {0, 1, 2};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  scoped_ptr<PeriodicKeyFrameSource> sources[kNumFlows];
  scoped_ptr<PacedVideoSender> senders[kNumFlows];

  for (size_t i = 0; i < kNumFlows; ++i) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    sources[i].reset(new PeriodicKeyFrameSource(kAllFlowIds[i], 30, 300, 0,
                                                i * 20000, 1000));
    senders[i].reset(new PacedVideoSender(this, sources[i].get(), GetParam()));
  }

  ChokeFilter choke(this, CreateFlowIds(kAllFlowIds, kNumFlows));
  choke.SetCapacity(1000);

  scoped_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(new RateCounterFilter(
        this, CreateFlowIds(&kAllFlowIds[i], 1), "receiver_input"));
  }

  RateCounterFilter total_utilization(
      this, CreateFlowIds(kAllFlowIds, kNumFlows), "total_utilization");

  scoped_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    receivers[i].reset(
        new PacketReceiver(this, kAllFlowIds[i], GetParam(), i == 0, false));
  }

  RunFor(30 * 60 * 1000);
}
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
