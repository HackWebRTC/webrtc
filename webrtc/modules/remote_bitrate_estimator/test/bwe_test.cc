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
class BweReceiver {
 public:
  explicit BweReceiver(int flow_id) : flow_id_(flow_id) {}
  virtual ~BweReceiver() {}

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             size_t payload_size,
                             const RTPHeader& header) {}
  virtual FeedbackPacket* GetFeedback(int64_t now_ms) { return NULL; }

 protected:
  int flow_id_;
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time) {
  const int kInterArrivalShift = 26;
  const int kAbsSendTimeInterArrivalUpshift = 8;
  const double kTimestampToMs =
      1000.0 / static_cast<double>(1 << kInterArrivalShift);
  uint32_t timestamp = abs_send_time << kAbsSendTimeInterArrivalUpshift;
  return static_cast<int64_t>(timestamp) * kTimestampToMs;
}

class SendSideBweReceiver : public BweReceiver {
 public:
  explicit SendSideBweReceiver(int flow_id)
      : BweReceiver(flow_id), last_feedback_ms_(0) {}
  virtual void ReceivePacket(int64_t arrival_time_ms,
                             size_t payload_size,
                             const RTPHeader& header) OVERRIDE {
    packet_feedback_vector_.push_back(PacketInfo(
        arrival_time_ms, GetAbsSendTimeInMs(header.extension.absoluteSendTime),
        header.sequenceNumber, payload_size));
  }

  virtual FeedbackPacket* GetFeedback(int64_t now_ms) OVERRIDE {
    if (now_ms - last_feedback_ms_ < 100)
      return NULL;
    last_feedback_ms_ = now_ms;
    FeedbackPacket* fb = new SendSideBweFeedback(flow_id_, now_ms * 1000,
                                                 packet_feedback_vector_);
    packet_feedback_vector_.clear();
    return fb;
  }

 private:
  int64_t last_feedback_ms_;
  std::vector<PacketInfo> packet_feedback_vector_;
};

class RembReceiver : public BweReceiver, public RemoteBitrateObserver {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RembReceiver(int flow_id, bool plot)
      : BweReceiver(flow_id),
        estimate_log_prefix_(),
        plot_estimate_(plot),
        clock_(0),
        recv_stats_(ReceiveStatistics::Create(&clock_)),
        latest_estimate_bps_(-1),
        estimator_(AbsoluteSendTimeRemoteBitrateEstimatorFactory().Create(
            this,
            &clock_,
            kAimdControl,
            kRemoteBitrateEstimatorMinBitrateBps)) {
    std::stringstream ss;
    ss << "Estimate_" << flow_id_ << "#1";
    estimate_log_prefix_ = ss.str();
    // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
    estimator_->OnRttUpdate(50);
  }

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             size_t payload_size,
                             const RTPHeader& header) {
    recv_stats_->IncomingPacket(header, payload_size, false);

    latest_estimate_bps_ = -1;

    int64_t step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    while ((clock_.TimeInMilliseconds() + step_ms) < arrival_time_ms) {
      clock_.AdvanceTimeMilliseconds(step_ms);
      estimator_->Process();
      step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    }
    estimator_->IncomingPacket(arrival_time_ms, payload_size, header);
    clock_.AdvanceTimeMilliseconds(arrival_time_ms -
                                   clock_.TimeInMilliseconds());
    ASSERT_TRUE(arrival_time_ms == clock_.TimeInMilliseconds());
  }

  virtual FeedbackPacket* GetFeedback(int64_t now_ms) OVERRIDE {
    BWE_TEST_LOGGING_CONTEXT("Remb");
    uint32_t estimated_bps = 0;
    RembFeedback* feedback = NULL;
    if (LatestEstimate(&estimated_bps)) {
      StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
      RTCPReportBlock report_block;
      if (!statisticians.empty()) {
        report_block = BuildReportBlock(statisticians.begin()->second);
      }
      feedback = new RembFeedback(flow_id_, now_ms * 1000, estimated_bps,
                                  report_block);

      double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
      RTC_UNUSED(estimated_kbps);
      if (plot_estimate_) {
        BWE_TEST_LOGGING_PLOT(estimate_log_prefix_, clock_.TimeInMilliseconds(),
                              estimated_kbps);
      }
    }
    return feedback;
  }

  // Implements RemoteBitrateObserver.
  virtual void OnReceiveBitrateChanged(const vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) OVERRIDE {}

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

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(flow_id, plot);
    case kFullSendSideEstimator:
      return new SendSideBweReceiver(flow_id);
    case kNullEstimator:
      return new BweReceiver(flow_id);
  }
  assert(false);
  return NULL;
}

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe)
    : PacketProcessor(listener, flow_id, kReceiver),
      delay_log_prefix_(),
      last_delay_plot_ms_(0),
      plot_delay_(plot_delay),
      bwe_receiver_(CreateBweReceiver(bwe_type, flow_id, plot_bwe)) {
  // Setup the prefix strings used when logging.
  std::stringstream ss;
  ss << "Delay_" << flow_id << "#2";
  delay_log_prefix_ = ss.str();
}

PacketReceiver::~PacketReceiver() {
}

void PacketReceiver::RunFor(int64_t time_ms, Packets* in_out) {
  Packets feedback;
  for (auto it = in_out->begin(); it != in_out->end();) {
    // PacketReceivers are only associated with a single stream, and therefore
    // should only process a single flow id.
    // TODO(holmer): Break this out into a Demuxer which implements both
    // PacketProcessorListener and PacketProcessor.
    if ((*it)->GetPacketType() == Packet::kMedia &&
        (*it)->flow_id() == *flow_ids().begin()) {
      BWE_TEST_LOGGING_CONTEXT("Receiver");
      const MediaPacket* media_packet = static_cast<const MediaPacket*>(*it);
      // We're treating the send time (from previous filter) as the arrival
      // time once packet reaches the estimator.
      int64_t arrival_time_ms = (media_packet->send_time_us() + 500) / 1000;
      BWE_TEST_LOGGING_TIME(arrival_time_ms);
      PlotDelay(arrival_time_ms,
                (media_packet->creation_time_us() + 500) / 1000);

      bwe_receiver_->ReceivePacket(arrival_time_ms,
                                   media_packet->payload_size(),
                                   media_packet->header());
      FeedbackPacket* fb = bwe_receiver_->GetFeedback(arrival_time_ms);
      if (fb)
        feedback.push_back(fb);
      delete media_packet;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  // Insert feedback packets to be sent back to the sender.
  in_out->merge(feedback, DereferencingComparator<Packet>);
}

void PacketReceiver::PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms) {
  static const int kDelayPlotIntervalMs = 100;
  if (!plot_delay_)
    return;
  if (arrival_time_ms - last_delay_plot_ms_ > kDelayPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT(delay_log_prefix_, arrival_time_ms,
                          arrival_time_ms - send_time_ms);
    last_delay_plot_ms_ = arrival_time_ms;
  }
}

class PacketProcessorRunner {
 public:
  explicit PacketProcessorRunner(PacketProcessor* processor)
      : processor_(processor) {}

  ~PacketProcessorRunner() {
    for (Packet* packet : queue_)
      delete packet;
  }

  bool RunsProcessor(const PacketProcessor* processor) const {
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
    : run_time_ms_(0), time_now_ms_(-1), simulation_interval_ms_(-1) {
  links_.push_back(&uplink_);
  links_.push_back(&downlink_);
}

BweTest::~BweTest() {
  for (Packet* packet : packets_)
    delete packet;
}

void BweTest::SetUp() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void Link::AddPacketProcessor(PacketProcessor* processor,
                              ProcessorType processor_type) {
  assert(processor);
  switch (processor_type) {
    case kSender:
      senders_.push_back(static_cast<PacketSender*>(processor));
      break;
    case kReceiver:
      receivers_.push_back(static_cast<PacketReceiver*>(processor));
      break;
    case kRegular:
      break;
  }
  processors_.push_back(PacketProcessorRunner(processor));
}

void Link::RemovePacketProcessor(PacketProcessor* processor) {
  for (vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->RunsProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

// Ownership of the created packets is handed over to the caller.
void Link::Run(int64_t run_for_ms, int64_t now_ms, Packets* packets) {
  for (auto& processor : processors_) {
    processor.RunFor(run_for_ms, now_ms, packets);
  }
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.
  if (!uplink_.senders().empty()) {
    simulation_interval_ms_ = uplink_.senders()[0]->GetFeedbackIntervalMs();
  } else if (!downlink_.senders().empty()) {
    simulation_interval_ms_ = downlink_.senders()[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    // Packets are first generated on the first link, passed through all the
    // PacketProcessors and PacketReceivers. The PacketReceivers produces
    // FeedbackPackets which are then processed by the next link, where they
    // at some point will be consumed by a PacketSender.
    for (Link* link : links_)
      link->Run(simulation_interval_ms_, time_now_ms_, &packets_);
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
