/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {
enum Estimator { kAbsSendTime, kTransmissionOffset };

BweTestConfig::EstimatorConfig EstimatorConfigs(Estimator estimator,
                                                int flow_id) {
  static const RemoteBitrateEstimatorFactory factories[] = {
    RemoteBitrateEstimatorFactory(),
    AbsoluteSendTimeRemoteBitrateEstimatorFactory()
  };
  switch (estimator) {
    case kTransmissionOffset:
      return BweTestConfig::EstimatorConfig("TOF", flow_id, &factories[0], true,
                                            true);
    case kAbsSendTime:
      return BweTestConfig::EstimatorConfig("AST", flow_id, &factories[1], true,
                                            true);
    default:
      assert(false);
      return BweTestConfig::EstimatorConfig("AST", flow_id, &factories[1], true,
                                            true);
  }
}

struct DefaultBweTestConfig {
  BweTestConfig bwe_test_config;
  size_t number_of_senders;
};

DefaultBweTestConfig MakeBweTestConfig(uint32_t sender_count,
                                       Estimator estimator) {
  DefaultBweTestConfig result;
  result.bwe_test_config.estimator_configs.push_back(
      EstimatorConfigs(estimator, 0));
  result.number_of_senders = sender_count;
  return result;
}

class DefaultBweTest : public BweTest,
                       public ::testing::TestWithParam<DefaultBweTestConfig> {
 public:
  DefaultBweTest() : packet_senders_() {}
  virtual ~DefaultBweTest() {}

  virtual void SetUp() {
    const DefaultBweTestConfig& config = GetParam();
    SetupTestFromConfig(config.bwe_test_config);
    for (size_t i = 0; i < config.number_of_senders; ++i) {
      packet_senders_.push_back(new VideoSender(0, this, 30, 300, 0, 0));
    }
  }

  virtual void TearDown() {
    while (packet_senders_.empty()) {
      delete packet_senders_.front();
      packet_senders_.pop_front();
    }
  }

 protected:
  std::list<PacketSender*> packet_senders_;
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest, DefaultBweTest,
    ::testing::Values(MakeBweTestConfig(1, kAbsSendTime),
                      MakeBweTestConfig(3, kAbsSendTime),
                      MakeBweTestConfig(1, kTransmissionOffset),
                      MakeBweTestConfig(3, kTransmissionOffset)));

TEST_P(DefaultBweTest, UnlimitedSpeed) {
  VerboseLogging(false);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, DISABLED_SteadyLoss) {
  LossFilter loss(this);
  loss.SetLoss(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingLoss1) {
  LossFilter loss(this);
  for (int i = 0; i < 76; ++i) {
    loss.SetLoss(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, SteadyDelay) {
  DelayFilter delay(this);
  delay.SetDelay(1000);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, DISABLED_IncreasingDelay1) {
  DelayFilter delay(this);
  RunFor(10 * 60 * 1000);
  for (int i = 0; i < 30 * 2; ++i) {
    delay.SetDelay(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingDelay2) {
  DelayFilter delay(this);
  RateCounterFilter counter(this);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(10.0f * i);
    RunFor(10 * 1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, JumpyDelay1) {
  DelayFilter delay(this);
  RunFor(10 * 60 * 1000);
  for (int i = 1; i < 200; ++i) {
    delay.SetDelay((10 * i) % 500);
    RunFor(1000);
    delay.SetDelay(1.0f);
    RunFor(1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, SteadyJitter) {
  JitterFilter jitter(this);
  RateCounterFilter counter(this);
  jitter.SetJitter(20);
  RunFor(2 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingJitter1) {
  JitterFilter jitter(this);
  for (int i = 0; i < 2 * 60 * 2; ++i) {
    jitter.SetJitter(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingJitter2) {
  JitterFilter jitter(this);
  RunFor(30 * 1000);
  for (int i = 1; i < 51; ++i) {
    jitter.SetJitter(10.0f * i);
    RunFor(10 * 1000);
  }
  jitter.SetJitter(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, SteadyReorder) {
  ReorderFilter reorder(this);
  reorder.SetReorder(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, IncreasingReorder1) {
  ReorderFilter reorder(this);
  for (int i = 0; i < 76; ++i) {
    reorder.SetReorder(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, DISABLED_SteadyChoke) {
  ChokeFilter choke(this);
  choke.SetCapacity(140);
  RunFor(10 * 60 * 1000);
}

TEST_P(DefaultBweTest, DISABLED_IncreasingChoke1) {
  ChokeFilter choke(this);
  for (int i = 1200; i >= 100; i -= 100) {
    choke.SetCapacity(i);
    RunFor(5000);
  }
}

TEST_P(DefaultBweTest, DISABLED_IncreasingChoke2) {
  ChokeFilter choke(this);
  RunFor(60 * 1000);
  for (int i = 1200; i >= 100; i -= 20) {
    choke.SetCapacity(i);
    RunFor(1000);
  }
}

TEST_P(DefaultBweTest, DISABLED_Multi1) {
  DelayFilter delay(this);
  ChokeFilter choke(this);
  RateCounterFilter counter(this);
  choke.SetCapacity(1000);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(100.0f * i);
    RunFor(10 * 1000);
  }
  RunFor(500 * 1000);
  delay.SetDelay(0.0f);
  RunFor(5 * 60 * 1000);
}

TEST_P(DefaultBweTest, Multi2) {
  ChokeFilter choke(this);
  JitterFilter jitter(this);
  RateCounterFilter counter(this);
  choke.SetCapacity(2000);
  jitter.SetJitter(120);
  RunFor(5 * 60 * 1000);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
