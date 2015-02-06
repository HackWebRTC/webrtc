/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"

#include "webrtc/base/common.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_baselinefile.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using std::string;
using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

class PacketReceiver {
 public:
  PacketReceiver(const string& test_name,
                 const BweTestConfig::EstimatorConfig& config)
      : flow_id_(config.flow_id),
        debug_name_(config.debug_name),
        delay_log_prefix_(),
        last_delay_plot_ms_(0),
        plot_delay_(config.plot_delay),
        baseline_(BaseLineFileInterface::Create(test_name + "_" + debug_name_,
                                                config.update_baseline)) {
    // Setup the prefix strings used when logging.
    std::stringstream ss;
    ss << "Delay_" << config.flow_id << "#2";
    delay_log_prefix_ = ss.str();
  }
  virtual ~PacketReceiver() {}

  virtual void ReceivePacket(const Packet& packet) {}

  virtual FeedbackPacket* GetFeedback() { return NULL; }

  void LogStats() {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    BWE_TEST_LOGGING_CONTEXT("Mean");
    stats_.Log("kbps");
  }

  void VerifyOrWriteBaseline() { EXPECT_TRUE(baseline_->VerifyOrWrite()); }

 protected:
  static const int kDelayPlotIntervalMs = 100;

  void LogDelay(int64_t arrival_time_ms, int64_t send_time_ms) {
    if (plot_delay_) {
      if (arrival_time_ms - last_delay_plot_ms_ > kDelayPlotIntervalMs) {
        BWE_TEST_LOGGING_PLOT(delay_log_prefix_, arrival_time_ms,
                              arrival_time_ms - send_time_ms);
        last_delay_plot_ms_ = arrival_time_ms;
      }
    }
  }

  const int flow_id_;
  const string debug_name_;
  string delay_log_prefix_;
  int64_t last_delay_plot_ms_;
  bool plot_delay_;
  scoped_ptr<BaseLineFileInterface> baseline_;
  Stats<double> stats_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PacketReceiver);
};

class SendSideBweReceiver : public PacketReceiver {
 public:
  SendSideBweReceiver(const string& test_name,
                      const BweTestConfig::EstimatorConfig& config)
      : PacketReceiver(test_name, config) {}

  virtual void EatPacket(const Packet& packet) {
    const MediaPacket& media_packet = static_cast<const MediaPacket&>(packet);
    // We're treating the send time (from previous filter) as the arrival
    // time once packet reaches the estimator.
    int64_t arrival_time_ms = (media_packet.send_time_us() + 500) / 1000;
    BWE_TEST_LOGGING_TIME(arrival_time_ms);
    LogDelay(arrival_time_ms, (media_packet.creation_time_us() + 500) / 1000);
    packet_feedback_vector_.push_back(PacketInfo(
        arrival_time_ms, media_packet.GetAbsSendTimeInMs(),
        media_packet.header().sequenceNumber, media_packet.payload_size()));
  }

  virtual FeedbackPacket* GetFeedback() {
    FeedbackPacket* fb =
        new SendSideBweFeedback(flow_id_, 0, packet_feedback_vector_);
    packet_feedback_vector_.clear();
    return fb;
  }

 private:
  std::vector<PacketInfo> packet_feedback_vector_;
};

class RembReceiver : public PacketReceiver, public RemoteBitrateObserver {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RembReceiver(const string& test_name,
               const BweTestConfig::EstimatorConfig& config)
      : PacketReceiver(test_name, config),
        estimate_log_prefix_(),
        plot_estimate_(config.plot_estimate),
        clock_(0),
        recv_stats_(ReceiveStatistics::Create(&clock_)),
        latest_estimate_bps_(-1),
        estimator_(config.estimator_factory->Create(
            this,
            &clock_,
            config.control_type,
            kRemoteBitrateEstimatorMinBitrateBps)) {
    assert(estimator_.get());
    assert(baseline_.get());
    std::stringstream ss;
    ss.str("");
    ss << "Estimate_" << config.flow_id << "#1";
    estimate_log_prefix_ = ss.str();
    // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
    estimator_->OnRttUpdate(50);
  }

  virtual void ReceivePacket(const Packet& packet) {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    assert(packet.GetPacketType() == Packet::kMediaPacket);
    const MediaPacket& media_packet = static_cast<const MediaPacket&>(packet);
    // We're treating the send time (from previous filter) as the arrival
    // time once packet reaches the estimator.
    int64_t arrival_time_ms = (media_packet.send_time_us() + 500) / 1000;
    BWE_TEST_LOGGING_TIME(arrival_time_ms);
    LogDelay(arrival_time_ms, (media_packet.creation_time_us() + 500) / 1000);
    recv_stats_->IncomingPacket(media_packet.header(),
                                media_packet.payload_size(), false);

    latest_estimate_bps_ = -1;

    int64_t step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    while ((clock_.TimeInMilliseconds() + step_ms) < arrival_time_ms) {
      clock_.AdvanceTimeMilliseconds(step_ms);
      estimator_->Process();
      step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    }
    estimator_->IncomingPacket(arrival_time_ms, media_packet.payload_size(),
                               media_packet.header());
    clock_.AdvanceTimeMilliseconds(arrival_time_ms -
                                   clock_.TimeInMilliseconds());
    ASSERT_TRUE(arrival_time_ms == clock_.TimeInMilliseconds());
  }

  virtual FeedbackPacket* GetFeedback() {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    uint32_t estimated_bps = 0;
    RembFeedback* feedback = NULL;
    if (LatestEstimate(&estimated_bps)) {
      StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
      RTCPReportBlock report_block;
      if (!statisticians.empty()) {
        report_block = BuildReportBlock(statisticians.begin()->second);
      }
      feedback = new RembFeedback(flow_id_, clock_.TimeInMilliseconds(),
                                  estimated_bps, report_block);
      baseline_->Estimate(clock_.TimeInMilliseconds(), estimated_bps);

      double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
      stats_.Push(estimated_kbps);
      if (plot_estimate_) {
        BWE_TEST_LOGGING_PLOT(estimate_log_prefix_, clock_.TimeInMilliseconds(),
                              estimated_kbps);
      }
    }
    return feedback;
  }

  virtual void OnReceiveBitrateChanged(const vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
  }

 private:
  static RTCPReportBlock BuildReportBlock(StreamStatistician* statistician) {
    RTCPReportBlock report_block;
    RtcpStatistics stats;
    if (!statistician->GetStatistics(&stats, true))
      return report_block;
    report_block.fractionLost = stats.fraction_lost;
    report_block.cumulativeLost = stats.cumulative_lost;
    report_block.extendedHighSeqNum = stats.extended_max_sequence_number;
    report_block.jitter = stats.jitter;
    return report_block;
  }

  bool LatestEstimate(uint32_t* estimate_bps) {
    if (latest_estimate_bps_ < 0) {
      vector<unsigned int> ssrcs;
      unsigned int bps = 0;
      if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
        return false;
      }
      latest_estimate_bps_ = bps;
    }
    *estimate_bps = latest_estimate_bps_;
    return true;
  }

  string estimate_log_prefix_;
  bool plot_estimate_;
  SimulatedClock clock_;
  scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t latest_estimate_bps_;
  scoped_ptr<RemoteBitrateEstimator> estimator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RembReceiver);
};

PacketReceiver* CreatePacketReceiver(
    BandwidthEstimatorType type,
    const string& test_name,
    const BweTestConfig::EstimatorConfig& config) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(test_name, config);
    case kFullSendSideEstimator:
      return new SendSideBweReceiver(test_name, config);
    case kNullEstimator:
      return new PacketReceiver(test_name, config);
  }
  assert(false);
  return NULL;
}

class PacketProcessorRunner {
 public:
  explicit PacketProcessorRunner(PacketProcessor* processor)
      : processor_(processor) {}

  ~PacketProcessorRunner() {
    for (Packet* packet : queue_)
      delete packet;
  }

  bool HasProcessor(const PacketProcessor* processor) const {
    return processor == processor_;
  }

  void RunFor(int64_t time_ms, int64_t time_now_ms, Packets* in_out) {
    Packets to_process;
    FindPacketsToProcess(processor_->flow_ids(), in_out, &to_process);
    processor_->RunFor(time_ms, &to_process);
    QueuePackets(&to_process, time_now_ms * 1000);
    if (!to_process.empty()) {
      processor_->Plot((to_process.back()->send_time_us() + 500) / 1000);
    }
    in_out->merge(to_process, DereferencingComparator<Packet>);
  }

 private:
  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in,
                            Packets* out) {
    assert(out->empty());
    for (Packets::iterator it = in->begin(); it != in->end();) {
      // TODO(holmer): Further optimize this by looking for consecutive flow ids
      // in the packet list and only doing the binary search + splice once for a
      // sequence.
      if (flow_ids.find((*it)->flow_id()) != flow_ids.end()) {
        Packets::iterator next = it;
        ++next;
        out->splice(out->end(), *in, it);
        it = next;
      } else {
        ++it;
      }
    }
  }

  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us) {
    queue_.merge(*batch, DereferencingComparator<Packet>);
    if (queue_.empty()) {
      return;
    }
    Packets::iterator it = queue_.begin();
    for (; it != queue_.end(); ++it) {
      if ((*it)->send_time_us() > end_of_batch_time_us) {
        break;
      }
    }
    Packets to_transfer;
    to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
    batch->merge(to_transfer, DereferencingComparator<Packet>);
  }

  PacketProcessor* processor_;
  Packets queue_;
};

BweTest::BweTest()
    : run_time_ms_(0),
      time_now_ms_(-1),
      simulation_interval_ms_(-1),
      estimators_(),
      processors_() {
}

BweTest::~BweTest() {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(true);
  for (const auto& estimator : estimators_) {
    estimator.second->VerifyOrWriteBaseline();
    estimator.second->LogStats();
  }
  BWE_TEST_LOGGING_GLOBAL_CONTEXT("");

  for (const auto& estimator : estimators_) {
    delete estimator.second;
  }
}

void BweTest::SetupTestFromConfig(const BweTestConfig& config) {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  for (const auto& estimator_config : config.estimator_configs) {
    estimators_.insert(
        std::make_pair(estimator_config.flow_id,
                       CreatePacketReceiver(estimator_config.bwe_type,
                                            test_name, estimator_config)));
  }
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void BweTest::AddPacketProcessor(PacketProcessor* processor, bool is_sender) {
  assert(processor);
  if (is_sender) {
    senders_.push_back(static_cast<PacketSender*>(processor));
  }
  processors_.push_back(PacketProcessorRunner(processor));
  for (const int& flow_id : processor->flow_ids()) {
    RTC_UNUSED(flow_id);
    assert(estimators_.count(flow_id) == 1);
  }
}

void BweTest::RemovePacketProcessor(PacketProcessor* processor) {
  for (vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->HasProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::GiveFeedbackToAffectedSenders(PacketReceiver* estimator) {
  FeedbackPacket* feedback = estimator->GetFeedback();
  if (feedback) {
    for (PacketSender* sender : senders_) {
      if (sender->flow_ids().find(feedback->flow_id()) !=
          sender->flow_ids().end()) {
        sender->GiveFeedback(*feedback);
        break;
      }
    }
  }
  delete feedback;
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.
  if (!senders_.empty()) {
    simulation_interval_ms_ = senders_[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    Packets packets;
    for (PacketProcessorRunner& processor : processors_) {
      processor.RunFor(simulation_interval_ms_, time_now_ms_, &packets);
    }

    // Verify packets are in order between batches.
    if (!packets.empty()) {
      if (!previous_packets_.empty()) {
        packets.splice(packets.begin(), previous_packets_,
                       --previous_packets_.end());
        ASSERT_TRUE(IsTimeSorted(packets));
        delete packets.front();
        packets.erase(packets.begin());
      }
      ASSERT_LE(packets.front()->send_time_us(), time_now_ms_ * 1000);
      ASSERT_LE(packets.back()->send_time_us(), time_now_ms_ * 1000);
    } else {
      ASSERT_TRUE(IsTimeSorted(packets));
    }

    for (const Packet* packet : packets) {
      EstimatorMap::iterator est_it = estimators_.find(packet->flow_id());
      ASSERT_TRUE(est_it != estimators_.end());
      est_it->second->ReceivePacket(*packet);
      delete packet;
    }

    for (const auto& estimator : estimators_) {
      GiveFeedbackToAffectedSenders(estimator.second);
    }
  }
}

string BweTest::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return string(test_info->name());
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
