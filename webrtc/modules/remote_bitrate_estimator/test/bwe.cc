/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

const int kMinBitrateKbps = 150;
const int kMaxBitrateKbps = 2000;

class NullBweSender : public BweSender {
 public:
  NullBweSender() {}
  virtual ~NullBweSender() {}

  virtual int GetFeedbackIntervalMs() const OVERRIDE { return 1000; }
  virtual void GiveFeedback(const FeedbackPacket& feedback) OVERRIDE {}
  virtual int64_t TimeUntilNextProcess() OVERRIDE {
    return std::numeric_limits<int64_t>::max();
  }
  virtual int Process() OVERRIDE { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullBweSender);
};

class RembBweSender : public BweSender {
 public:
  RembBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~RembBweSender();

  virtual int GetFeedbackIntervalMs() const OVERRIDE { return 100; }
  virtual void GiveFeedback(const FeedbackPacket& feedback) OVERRIDE;
  virtual int64_t TimeUntilNextProcess() OVERRIDE;
  virtual int Process() OVERRIDE;

 protected:
  scoped_ptr<BitrateController> bitrate_controller_;
  scoped_ptr<RtcpBandwidthObserver> feedback_observer_;

 private:
  Clock* clock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RembBweSender);
};

class FullBweSender : public BweSender, public RemoteBitrateObserver {
 public:
  FullBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~FullBweSender();

  virtual int GetFeedbackIntervalMs() const OVERRIDE { return 100; }
  virtual void GiveFeedback(const FeedbackPacket& feedback) OVERRIDE;
  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) OVERRIDE;
  virtual int64_t TimeUntilNextProcess() OVERRIDE;
  virtual int Process() OVERRIDE;

 protected:
  scoped_ptr<BitrateController> bitrate_controller_;
  scoped_ptr<RemoteBitrateEstimator> rbe_;
  scoped_ptr<RtcpBandwidthObserver> feedback_observer_;

 private:
  Clock* const clock_;
  RTCPReportBlock report_block_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FullBweSender);
};

RembBweSender::RembBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, false)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetBitrateObserver(
      observer, 1000 * kbps, 1000 * kMinBitrateKbps, 1000 * kMaxBitrateKbps);
}

RembBweSender::~RembBweSender() {
}

void RembBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const RembFeedback& remb_feedback =
      static_cast<const RembFeedback&>(feedback);
  feedback_observer_->OnReceivedEstimatedBitrate(remb_feedback.estimated_bps());
  ReportBlockList report_blocks;
  report_blocks.push_back(remb_feedback.report_block());
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

int64_t RembBweSender::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int RembBweSender::Process() {
  return bitrate_controller_->Process();
}

FullBweSender::FullBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, false)),
      rbe_(AbsoluteSendTimeRemoteBitrateEstimatorFactory()
               .Create(this, clock, kAimdControl, 1000 * kMinBitrateKbps)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetBitrateObserver(
      observer, 1000 * kbps, 1000 * kMinBitrateKbps, 1000 * kMaxBitrateKbps);
}

FullBweSender::~FullBweSender() {
}

void FullBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const SendSideBweFeedback& fb =
      static_cast<const SendSideBweFeedback&>(feedback);
  if (fb.packet_feedback_vector().empty())
    return;
  rbe_->IncomingPacketFeedbackVector(fb.packet_feedback_vector());
  // TODO(holmer): Handle losses in between feedback packets.
  int expected_packets = fb.packet_feedback_vector().back().sequence_number -
                         fb.packet_feedback_vector().front().sequence_number +
                         1;
  // Assuming no reordering for now.
  if (expected_packets <= 0)
    return;
  int lost_packets = expected_packets - fb.packet_feedback_vector().size();
  report_block_.fractionLost = (lost_packets << 8) / expected_packets;
  report_block_.cumulativeLost += lost_packets;
  ReportBlockList report_blocks;
  report_blocks.push_back(report_block_);
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

void FullBweSender::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs,
    unsigned int bitrate) {
  feedback_observer_->OnReceivedEstimatedBitrate(bitrate);
}

int64_t FullBweSender::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int FullBweSender::Process() {
  rbe_->Process();
  return bitrate_controller_->Process();
}

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
                             const MediaPacket& media_packet) OVERRIDE {
    packet_feedback_vector_.push_back(PacketInfo(
        arrival_time_ms,
        GetAbsSendTimeInMs(media_packet.header().extension.absoluteSendTime),
        media_packet.header().sequenceNumber, media_packet.payload_size()));
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
                             const MediaPacket& media_packet) {
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
  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
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
      std::vector<unsigned int> ssrcs;
      unsigned int bps = 0;
      if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
        return false;
      }
      latest_estimate_bps_ = bps;
    }
    *estimate_bps = latest_estimate_bps_;
    return true;
  }

  std::string estimate_log_prefix_;
  bool plot_estimate_;
  SimulatedClock clock_;
  scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t latest_estimate_bps_;
  scoped_ptr<RemoteBitrateEstimator> estimator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RembReceiver);
};

class NadaBweReceiver : public BweReceiver {
 public:
  explicit NadaBweReceiver(int flow_id)
      : BweReceiver(flow_id),
        clock_(0),
        last_feedback_ms_(0),
        recv_stats_(ReceiveStatistics::Create(&clock_)),
        baseline_delay_ms_(0),
        delay_signal_ms_(0),
        last_congestion_signal_ms_(0) {}

  virtual void ReceivePacket(int64_t arrival_time_ms,
                             const MediaPacket& media_packet) OVERRIDE {
    clock_.AdvanceTimeMilliseconds(arrival_time_ms -
                                   clock_.TimeInMilliseconds());
    recv_stats_->IncomingPacket(media_packet.header(),
                                media_packet.payload_size(), false);
    int64_t delay_ms = arrival_time_ms - media_packet.creation_time_us() / 1000;
    // TODO(holmer): The min should time out after 10 minutes.
    if (delay_ms < baseline_delay_ms_) {
      baseline_delay_ms_ = delay_ms;
    }
    delay_signal_ms_ = delay_ms - baseline_delay_ms_;
  }

  virtual FeedbackPacket* GetFeedback(int64_t now_ms) OVERRIDE {
    if (now_ms - last_feedback_ms_ < 100)
      return NULL;

    StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
    int64_t loss_signal_ms = 0.0f;
    if (!statisticians.empty()) {
      RtcpStatistics stats;
      if (!statisticians.begin()->second->GetStatistics(&stats, true)) {
        const float kLossSignalWeight = 1000.0f;
        loss_signal_ms =
            (kLossSignalWeight * static_cast<float>(stats.fraction_lost) +
             127) /
            255;
      }
    }

    int64_t congestion_signal_ms = delay_signal_ms_ + loss_signal_ms;

    float derivative = 0.0f;
    if (last_feedback_ms_ > 0) {
      derivative = (congestion_signal_ms - last_congestion_signal_ms_) /
                   static_cast<float>(now_ms - last_feedback_ms_);
    }
    last_feedback_ms_ = now_ms;
    last_congestion_signal_ms_ = congestion_signal_ms;
    return new NadaFeedback(flow_id_, now_ms, congestion_signal_ms, derivative);
  }

 private:
  SimulatedClock clock_;
  int64_t last_feedback_ms_;
  scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t baseline_delay_ms_;
  int64_t delay_signal_ms_;
  int64_t last_congestion_signal_ms_;
};

class NadaBweSender : public BweSender {
 public:
  NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock)
      : clock_(clock),
        observer_(observer),
        bitrate_kbps_(kbps),
        last_feedback_ms_(0) {}
  virtual ~NadaBweSender() {}

  virtual int GetFeedbackIntervalMs() const OVERRIDE { return 100; }

  virtual void GiveFeedback(const FeedbackPacket& feedback) {
    const NadaFeedback& fb = static_cast<const NadaFeedback&>(feedback);

    // TODO(holmer): Implement special start-up behavior.

    const float kEta = 2.0f;
    const float kTaoO = 500.0f;
    float x_hat = fb.congestion_signal() + kEta * kTaoO * fb.derivative();

    int64_t now_ms = clock_->TimeInMilliseconds();
    float delta_s = now_ms - last_feedback_ms_;
    last_feedback_ms_ = now_ms;

    const float kPriorityWeight = 1.0f;
    const float kReferenceDelayS = 10.0f;
    float kTheta = kPriorityWeight * (kMaxBitrateKbps - kMinBitrateKbps) *
                   kReferenceDelayS;

    const float kKappa = 1.0f;
    bitrate_kbps_ = bitrate_kbps_ +
                    kKappa * delta_s / (kTaoO * kTaoO) *
                        (kTheta - (bitrate_kbps_ - kMinBitrateKbps) * x_hat) +
                    0.5f;
    bitrate_kbps_ = std::min(bitrate_kbps_, kMaxBitrateKbps);
    bitrate_kbps_ = std::max(bitrate_kbps_, kMinBitrateKbps);

    observer_->OnNetworkChanged(1000 * bitrate_kbps_, 0, 0);
  }

  virtual int64_t TimeUntilNextProcess() OVERRIDE { return 100; }
  virtual int Process() OVERRIDE { return 0; }

 private:
  Clock* const clock_;
  BitrateObserver* const observer_;
  int bitrate_kbps_;
  int64_t last_feedback_ms_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NadaBweSender);
};

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock) {
  switch (estimator) {
    case kRembEstimator:
      return new RembBweSender(kbps, observer, clock);
    case kFullSendSideEstimator:
      return new FullBweSender(kbps, observer, clock);
    case kNadaEstimator:
      return new NadaBweSender(kbps, observer, clock);
    case kNullEstimator:
      return new NullBweSender();
  }
  assert(false);
  return NULL;
}

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(flow_id, plot);
    case kFullSendSideEstimator:
      return new SendSideBweReceiver(flow_id);
    case kNadaEstimator:
      return new NadaBweReceiver(flow_id);
    case kNullEstimator:
      return new BweReceiver(flow_id);
  }
  assert(false);
  return NULL;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
