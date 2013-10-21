/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/modules/remote_bitrate_estimator/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

#define ENABLE_1_SENDER 1
#define ENABLE_3_SENDERS 1
#define ENABLE_10_SENDERS 1
#define ENABLE_BASIC_TESTS 1
#define ENABLE_LOSS_TESTS 0
#define ENABLE_DELAY_TESTS 0
#define ENABLE_JITTER_TESTS 0
#define ENABLE_REORDER_TESTS 0
#define ENABLE_CHOKE_TESTS 0
#define ENABLE_MULTI_TESTS 0

#define ENABLE_TOF_ESTIMATOR 1
#define ENABLE_AST_ESTIMATOR 1

using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

const int64_t kSimulationIntervalMs = 1000;

namespace stl_helpers {
template<typename T> void DeleteElements(T* container) {
  if (!container) return;
  for (typename T::iterator it = container->begin(); it != container->end();
      ++it) {
    delete *it;
  }
  container->clear();
}
}  // namespace stl_helpers

class TestedEstimator : public RemoteBitrateObserver {
 public:
  TestedEstimator(const std::string& debug_name,
                  const RemoteBitrateEstimatorFactory& factory)
      : debug_name_(debug_name),
        clock_(0),
        stats_(),
        relative_estimator_stats_(),
        latest_estimate_kbps_(-1.0),
        estimator_(factory.Create(this, &clock_)),
        relative_estimator_(NULL) {
    assert(estimator_.get());
    // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
    estimator_->OnRttUpdate(50);
  }

  void SetRelativeEstimator(TestedEstimator* relative_estimator) {
    relative_estimator_ = relative_estimator;
  }

  void EatPacket(const BwePacket& packet) {
    latest_estimate_kbps_ = -1.0;

    // We're treating the send time (from previous filter) as the arrival
    // time once packet reaches the estimator.
    int64_t packet_time_ms = (packet.send_time_us() + 500) / 1000;

    int64_t step_ms = estimator_->TimeUntilNextProcess();
    while ((clock_.TimeInMilliseconds() + step_ms) < packet_time_ms) {
      clock_.AdvanceTimeMilliseconds(step_ms);
      estimator_->Process();
      step_ms = estimator_->TimeUntilNextProcess();
    }

    estimator_->IncomingPacket(packet_time_ms, packet.payload_size(),
                               packet.header());
    clock_.AdvanceTimeMilliseconds(packet_time_ms -
                                   clock_.TimeInMilliseconds());
    ASSERT_TRUE(packet_time_ms == clock_.TimeInMilliseconds());
  }

  void CheckEstimate() {
    double estimated_kbps = 0.0;
    if (LatestEstimate(&estimated_kbps)) {
      stats_.Push(estimated_kbps);
      double relative_estimate_kbps = 0.0;
      if (relative_estimator_ &&
          relative_estimator_->LatestEstimate(&relative_estimate_kbps)) {
        relative_estimator_stats_.Push(estimated_kbps - relative_estimate_kbps);
      }
    }
  }

  void LogStats() {
    printf("%s Mean ", debug_name_.c_str());
    stats_.Log("kbps");
    printf("\n");
    if (relative_estimator_) {
      printf("%s Diff ", debug_name_.c_str());
      relative_estimator_stats_.Log("kbps");
      printf("\n");
    }
  }

  virtual void OnReceiveBitrateChanged(const vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
  }

 private:
  bool LatestEstimate(double* estimate_kbps) {
    if (latest_estimate_kbps_ < 0.0) {
      vector<unsigned int> ssrcs;
      unsigned int bps = 0;
      if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
        return false;
      }
      latest_estimate_kbps_ = bps / 1000.0;
    }
    *estimate_kbps = latest_estimate_kbps_;
    return true;
  }

  std::string debug_name_;
  bool log_estimates_;
  SimulatedClock clock_;
  Stats<double> stats_;
  Stats<double> relative_estimator_stats_;
  double latest_estimate_kbps_;
  scoped_ptr<RemoteBitrateEstimator> estimator_;
  TestedEstimator* relative_estimator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TestedEstimator);
};

class RemoteBitrateEstimatorsTest : public ::testing::Test {
 public:
  RemoteBitrateEstimatorsTest()
      : run_time_ms_(0),
        estimators_(),
        previous_packets_(),
        processors_(),
        video_senders_() {
  }
  virtual ~RemoteBitrateEstimatorsTest() {
    stl_helpers::DeleteElements(&estimators_);
    stl_helpers::DeleteElements(&video_senders_);
  }

  virtual void SetUp() {
#if ENABLE_TOF_ESTIMATOR
    estimators_.push_back(new TestedEstimator("TOF",
        RemoteBitrateEstimatorFactory()));
#endif
#if ENABLE_AST_ESTIMATOR
    estimators_.push_back(new TestedEstimator("AST",
        AbsoluteSendTimeRemoteBitrateEstimatorFactory()));
#endif
    // Set all estimators as relative to the first one.
    for (uint32_t i = 1; i < estimators_.size(); ++i) {
      estimators_[i]->SetRelativeEstimator(estimators_[0]);
    }
  }

 protected:
  void RunFor(int64_t time_ms) {
    for (run_time_ms_ += time_ms; run_time_ms_ >= kSimulationIntervalMs;
        run_time_ms_ -= kSimulationIntervalMs) {
      Packets packets;
      for (vector<PacketProcessorInterface*>::const_iterator it =
           processors_.begin(); it != processors_.end(); ++it) {
        (*it)->RunFor(kSimulationIntervalMs, &packets);
      }

      // Verify packets are in order between batches.
      if (!packets.empty() && !previous_packets_.empty()) {
        packets.splice(packets.begin(), previous_packets_,
                       --previous_packets_.end());
        ASSERT_TRUE(IsTimeSorted(packets));
        packets.erase(packets.begin());
      } else {
        ASSERT_TRUE(IsTimeSorted(packets));
      }

      for (PacketsConstIt pit = packets.begin(); pit != packets.end(); ++pit) {
        for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
            eit != estimators_.end(); ++eit) {
          (*eit)->EatPacket(*pit);
        }
      }

      previous_packets_.swap(packets);

      for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
          eit != estimators_.end(); ++eit) {
        (*eit)->CheckEstimate();
      }
    }
  }

  void AddVideoSenders(uint32_t count) {
    struct { float fps; uint32_t kbps; uint32_t ssrc; float frame_offset; }
        configs[] = {
            { 30.00f, 150, 0x1234, 0.13f },
            { 15.00f, 500, 0x2345, 0.16f },
            { 30.00f, 1200, 0x3456, 0.26f },
            { 7.49f, 150, 0x4567, 0.05f },
            { 7.50f, 150, 0x5678, 0.15f },
            { 7.51f, 150, 0x6789, 0.25f },
            { 15.02f, 150, 0x7890, 0.27f },
            { 15.03f, 150, 0x8901, 0.38f },
            { 30.02f, 150, 0x9012, 0.39f },
            { 30.03f, 150, 0x0123, 0.52f }
        };
    assert(count <= sizeof(configs) / sizeof(configs[0]));
    uint32_t total_capacity = 0;
    for (uint32_t i = 0; i < count; ++i) {
      video_senders_.push_back(new VideoSender(configs[i].fps, configs[i].kbps,
          configs[i].ssrc, configs[i].frame_offset));
      processors_.push_back(video_senders_.back());
      total_capacity += configs[i].kbps;
    }
    printf("RequiredLinkCapacity %d kbps\n", total_capacity);
  }

  void LogStats() {
    for (vector<TestedEstimator*>::iterator eit = estimators_.begin();
        eit != estimators_.end(); ++eit) {
      (*eit)->LogStats();
    }
  }

  void UnlimitedSpeedTest() {
    RunFor(10 * 60 * 1000);
  }

  void SteadyLossTest() {
    LossFilter loss;
    processors_.push_back(&loss);
    loss.SetLoss(20.0);
    RunFor(10 * 60 * 1000);
  }
  void IncreasingLoss1Test() {
    LossFilter loss;
    processors_.push_back(&loss);
    for (int i = 0; i < 76; ++i) {
      loss.SetLoss(i);
      RunFor(5000);
    }
  }

  void SteadyDelayTest() {
    DelayFilter delay;
    processors_.push_back(&delay);
    delay.SetDelay(1000);
    RunFor(10 * 60 * 1000);
  }
  void IncreasingDelay1Test() {
    DelayFilter delay;
    processors_.push_back(&delay);
    RunFor(10 * 60 * 1000);
    for (int i = 0; i < 30 * 2; ++i) {
      delay.SetDelay(i);
      RunFor(10 * 1000);
    }
    RunFor(10 * 60 * 1000);
  }
  void IncreasingDelay2Test() {
    DelayFilter delay;
    RateCounterFilter counter;
    processors_.push_back(&delay);
    processors_.push_back(&counter);
    RunFor(1 * 60 * 1000);
    for (int i = 1; i < 51; ++i) {
      delay.SetDelay(10.0f * i);
      RunFor(10 * 1000);
    }
    delay.SetDelay(0.0f);
    RunFor(10 * 60 * 1000);
  }
  void JumpyDelay1Test() {
    DelayFilter delay;
    processors_.push_back(&delay);
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

  void SteadyJitterTest() {
    JitterFilter jitter;
    RateCounterFilter counter;
    processors_.push_back(&jitter);
    processors_.push_back(&counter);
    jitter.SetJitter(120);
    RunFor(10 * 60 * 1000);
  }
  void IncreasingJitter1Test() {
    JitterFilter jitter;
    processors_.push_back(&jitter);
    for (int i = 0; i < 2 * 60 * 2; ++i) {
      jitter.SetJitter(i);
      RunFor(10 * 1000);
    }
    RunFor(10 * 60 * 1000);
  }
  void IncreasingJitter2Test() {
    JitterFilter jitter;
    processors_.push_back(&jitter);
    RunFor(30 * 1000);
    for (int i = 1; i < 51; ++i) {
      jitter.SetJitter(10.0f * i);
      RunFor(10 * 1000);
    }
    jitter.SetJitter(0.0f);
    RunFor(10 * 60 * 1000);
  }

  void SteadyReorderTest() {
    ReorderFilter reorder;
    processors_.push_back(&reorder);
    reorder.SetReorder(20.0);
    RunFor(10 * 60 * 1000);
  }
  void IncreasingReorder1Test() {
    ReorderFilter reorder;
    processors_.push_back(&reorder);
    for (int i = 0; i < 76; ++i) {
      reorder.SetReorder(i);
      RunFor(5000);
    }
  }

  void SteadyChokeTest() {
    ChokeFilter choke;
    processors_.push_back(&choke);
    choke.SetCapacity(140);
    RunFor(10 * 60 * 1000);
  }
  void IncreasingChoke1Test() {
    ChokeFilter choke;
    processors_.push_back(&choke);
    for (int i = 1200; i >= 100; i -= 100) {
      choke.SetCapacity(i);
      RunFor(5000);
    }
  }
  void IncreasingChoke2Test() {
    ChokeFilter choke;
    processors_.push_back(&choke);
    RunFor(60 * 1000);
    for (int i = 1200; i >= 100; i -= 20) {
      choke.SetCapacity(i);
      RunFor(1000);
    }
  }

  void Multi1Test() {
    DelayFilter delay;
    ChokeFilter choke;
    RateCounterFilter counter;
    processors_.push_back(&delay);
    processors_.push_back(&choke);
    processors_.push_back(&counter);
    choke.SetCapacity(1000);
    RunFor(1 * 60 * 1000);
    for (int i = 1; i < 51; ++i) {
      delay.SetDelay(100.0f * i);
      RunFor(10 * 1000);
    }
    delay.SetDelay(0.0f);
    RunFor(5 * 60 * 1000);
  }
  void Multi2Test() {
    ChokeFilter choke;
    JitterFilter jitter;
    RateCounterFilter counter;
    processors_.push_back(&choke);
    processors_.push_back(&jitter);
    processors_.push_back(&counter);
    choke.SetCapacity(2000);
    jitter.SetJitter(120);
    RunFor(5 * 60 * 1000);
  }

 private:
  int64_t run_time_ms_;
  vector<TestedEstimator*> estimators_;
  Packets previous_packets_;
  vector<PacketProcessorInterface*> processors_;
  vector<VideoSender*> video_senders_;

  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorsTest);
};

#define SINGLE_TEST(enabled, test_name, video_senders)\
    TEST_F(RemoteBitrateEstimatorsTest, test_name##_##video_senders##Sender) {\
      if (enabled) {\
        AddVideoSenders(video_senders);\
        test_name##Test();\
        LogStats();\
      }\
    }

#define MULTI_TEST(enabled, test_name)\
    SINGLE_TEST((enabled) && ENABLE_1_SENDER, test_name, 1)\
    SINGLE_TEST((enabled) && ENABLE_3_SENDERS, test_name, 3)\
    SINGLE_TEST((enabled) && ENABLE_10_SENDERS, test_name, 10)

MULTI_TEST(ENABLE_BASIC_TESTS, UnlimitedSpeed)
MULTI_TEST(ENABLE_LOSS_TESTS, SteadyLoss)
MULTI_TEST(ENABLE_LOSS_TESTS, IncreasingLoss1)
MULTI_TEST(ENABLE_DELAY_TESTS, SteadyDelay)
MULTI_TEST(ENABLE_DELAY_TESTS, IncreasingDelay1)
MULTI_TEST(ENABLE_DELAY_TESTS, IncreasingDelay2)
MULTI_TEST(ENABLE_DELAY_TESTS, JumpyDelay1)
MULTI_TEST(ENABLE_JITTER_TESTS, SteadyJitter)
MULTI_TEST(ENABLE_JITTER_TESTS, IncreasingJitter1)
MULTI_TEST(ENABLE_JITTER_TESTS, IncreasingJitter2)
MULTI_TEST(ENABLE_REORDER_TESTS, SteadyReorder)
MULTI_TEST(ENABLE_REORDER_TESTS, IncreasingReorder1)
MULTI_TEST(ENABLE_CHOKE_TESTS, SteadyChoke)
MULTI_TEST(ENABLE_CHOKE_TESTS, IncreasingChoke1)
MULTI_TEST(ENABLE_CHOKE_TESTS, IncreasingChoke2)
MULTI_TEST(ENABLE_MULTI_TESTS, Multi1)
MULTI_TEST(ENABLE_MULTI_TESTS, Multi2)

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
