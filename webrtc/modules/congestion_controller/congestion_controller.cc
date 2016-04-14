/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/include/congestion_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/video/payload_router.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(RemoteBitrateObserver* observer, Clock* clock)
      : observer_(observer),
        clock_(clock),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        rbe_(new RemoteBitrateEstimatorSingleStream(observer_, clock_)),
        using_absolute_send_time_(false),
        packets_since_absolute_send_time_(0),
        min_bitrate_bps_(RemoteBitrateEstimator::kDefaultMinBitrateBps) {}

  virtual ~WrappingBitrateEstimator() {}

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header,
                      bool was_paced) override {
    CriticalSectionScoped cs(crit_sect_.get());
    PickEstimatorFromHeader(header);
    rbe_->IncomingPacket(arrival_time_ms, payload_size, header, was_paced);
  }

  void Process() override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->Process();
  }

  int64_t TimeUntilNextProcess() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->TimeUntilNextProcess();
  }

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  }

  void RemoveStream(unsigned int ssrc) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->RemoveStream(ssrc);
  }

  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->LatestEstimate(ssrcs, bitrate_bps);
  }

  void SetMinBitrate(int min_bitrate_bps) {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->SetMinBitrate(min_bitrate_bps);
    min_bitrate_bps_ = min_bitrate_bps;
  }

 private:
  void PickEstimatorFromHeader(const RTPHeader& header)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (header.extension.hasAbsoluteSendTime) {
      // If we see AST in header, switch RBE strategy immediately.
      if (!using_absolute_send_time_) {
        LOG(LS_INFO) <<
            "WrappingBitrateEstimator: Switching to absolute send time RBE.";
        using_absolute_send_time_ = true;
        PickEstimator();
      }
      packets_since_absolute_send_time_ = 0;
    } else {
      // When we don't see AST, wait for a few packets before going back to TOF.
      if (using_absolute_send_time_) {
        ++packets_since_absolute_send_time_;
        if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
          LOG(LS_INFO) << "WrappingBitrateEstimator: Switching to transmission "
                       << "time offset RBE.";
          using_absolute_send_time_ = false;
          PickEstimator();
        }
      }
    }
  }

  // Instantiate RBE for Time Offset or Absolute Send Time extensions.
  void PickEstimator() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (using_absolute_send_time_) {
      rbe_.reset(new RemoteBitrateEstimatorAbsSendTime(observer_));
    } else {
      rbe_.reset(new RemoteBitrateEstimatorSingleStream(observer_, clock_));
    }
    rbe_->SetMinBitrate(min_bitrate_bps_);
  }

  RemoteBitrateObserver* observer_;
  Clock* const clock_;
  std::unique_ptr<CriticalSectionWrapper> crit_sect_;
  std::unique_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;
  int min_bitrate_bps_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};

}  // namespace

CongestionController::CongestionController(
    Clock* clock,
    BitrateObserver* bitrate_observer,
    RemoteBitrateObserver* remote_bitrate_observer)
    : clock_(clock),
      pacer_(new PacedSender(clock_,
             &packet_router_,
             BitrateController::kDefaultStartBitrateKbps,
             PacedSender::kDefaultPaceMultiplier *
                 BitrateController::kDefaultStartBitrateKbps,
             0)),
      remote_bitrate_estimator_(
          new WrappingBitrateEstimator(remote_bitrate_observer, clock_)),
      // Constructed last as this object calls the provided callback on
      // construction.
      bitrate_controller_(
          BitrateController::CreateBitrateController(clock_, bitrate_observer)),
      remote_estimator_proxy_(clock_, &packet_router_),
      transport_feedback_adapter_(bitrate_controller_.get(), clock_),
      min_bitrate_bps_(RemoteBitrateEstimator::kDefaultMinBitrateBps) {
  transport_feedback_adapter_.SetBitrateEstimator(
      new RemoteBitrateEstimatorAbsSendTime(&transport_feedback_adapter_));
  transport_feedback_adapter_.GetBitrateEstimator()->SetMinBitrate(
      min_bitrate_bps_);
}

CongestionController::~CongestionController() {
}


void CongestionController::SetBweBitrates(int min_bitrate_bps,
                                          int start_bitrate_bps,
                                          int max_bitrate_bps) {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  const int kMinBitrateBps = 10000;
  if (min_bitrate_bps < kMinBitrateBps)
    min_bitrate_bps = kMinBitrateBps;
  if (max_bitrate_bps > 0)
    max_bitrate_bps = std::max(min_bitrate_bps, max_bitrate_bps);
  if (start_bitrate_bps > 0) {
    start_bitrate_bps = std::max(min_bitrate_bps, start_bitrate_bps);
    bitrate_controller_->SetStartBitrate(start_bitrate_bps);
  }
  bitrate_controller_->SetMinMaxBitrate(min_bitrate_bps, max_bitrate_bps);
  if (remote_bitrate_estimator_)
    remote_bitrate_estimator_->SetMinBitrate(min_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
  transport_feedback_adapter_.GetBitrateEstimator()->SetMinBitrate(
      min_bitrate_bps_);
}

BitrateController* CongestionController::GetBitrateController() const {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* CongestionController::GetRemoteBitrateEstimator(
    bool send_side_bwe) {
  if (send_side_bwe) {
    return &remote_estimator_proxy_;
  } else {
    return remote_bitrate_estimator_.get();
  }
}

TransportFeedbackObserver*
CongestionController::GetTransportFeedbackObserver() {
  return &transport_feedback_adapter_;
}

void CongestionController::UpdatePacerBitrate(int bitrate_kbps,
                                              int max_bitrate_kbps,
                                              int min_bitrate_kbps) {
  pacer_->UpdateBitrate(bitrate_kbps, max_bitrate_kbps, min_bitrate_kbps);
}

int64_t CongestionController::GetPacerQueuingDelayMs() const {
  return pacer_->QueueInMs();
}

void CongestionController::SignalNetworkState(NetworkState state) {
  if (state == kNetworkUp) {
    pacer_->Resume();
  } else {
    pacer_->Pause();
  }
}

void CongestionController::OnSentPacket(const rtc::SentPacket& sent_packet) {
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                            sent_packet.send_time_ms);
}

void CongestionController::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  remote_bitrate_estimator_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  transport_feedback_adapter_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

int64_t CongestionController::TimeUntilNextProcess() {
  return std::min(bitrate_controller_->TimeUntilNextProcess(),
                  remote_bitrate_estimator_->TimeUntilNextProcess());
}

void CongestionController::Process() {
  bitrate_controller_->Process();
  remote_bitrate_estimator_->Process();
}

}  // namespace webrtc
