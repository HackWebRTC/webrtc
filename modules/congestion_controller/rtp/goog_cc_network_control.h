/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_GOOG_CC_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_GOOG_CC_NETWORK_CONTROL_H_

#include <stdint.h>
#include <deque>
#include <memory>
#include <vector>

#include "api/optional.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "modules/congestion_controller/rtp/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/rtp/alr_detector.h"
#include "modules/congestion_controller/rtp/delay_based_bwe.h"
#include "modules/congestion_controller/rtp/network_control/include/network_control.h"
#include "modules/congestion_controller/rtp/probe_controller.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class GoogCcNetworkController : public NetworkControllerInterface {
 public:
  GoogCcNetworkController(RtcEventLog* event_log,
                          NetworkControllerObserver* observer);
  ~GoogCcNetworkController() override;

  // NetworkControllerInterface
  void OnNetworkAvailability(NetworkAvailability msg) override;
  void OnNetworkRouteChange(NetworkRouteChange msg) override;
  void OnProcessInterval(ProcessInterval msg) override;
  void OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  void OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  void OnSentPacket(SentPacket msg) override;
  void OnStreamsConfig(StreamsConfig msg) override;
  void OnTargetRateConstraints(TargetRateConstraints msg) override;
  void OnTransportLossReport(TransportLossReport msg) override;
  void OnTransportPacketsFeedback(TransportPacketsFeedback msg) override;

 private:
  void MaybeUpdateCongestionWindow();
  void MaybeTriggerOnNetworkChanged(Timestamp at_time);
  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms,
                            Timestamp at_time);
  void OnNetworkEstimate(NetworkEstimate msg);
  void UpdatePacingRates(Timestamp at_time);

  RtcEventLog* const event_log_;
  NetworkControllerObserver* const observer_;

  const std::unique_ptr<ProbeController> probe_controller_;

  std::unique_ptr<SendSideBandwidthEstimation> bandwidth_estimation_;
  std::unique_ptr<AlrDetector> alr_detector_;
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;

  std::deque<int64_t> feedback_rtts_;
  rtc::Optional<int64_t> min_feedback_rtt_ms_;

  rtc::Optional<NetworkEstimate> last_estimate_;
  rtc::Optional<TargetTransferRate> last_target_rate_;

  int32_t last_estimated_bitrate_bps_ = 0;
  uint8_t last_estimated_fraction_loss_ = 0;
  int64_t last_estimated_rtt_ms_ = 0;

  double pacing_factor_;
  DataRate min_pacing_rate_;
  DataRate max_padding_rate_;

  bool in_cwnd_experiment_;
  int64_t accepted_queue_ms_;
  bool previously_in_alr = false;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(GoogCcNetworkController);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_GOOG_CC_NETWORK_CONTROL_H_
