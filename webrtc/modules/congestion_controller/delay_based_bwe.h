/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/inter_arrival.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

class DelayBasedBwe : public RemoteBitrateEstimator {
 public:
  DelayBasedBwe(RemoteBitrateObserver* observer, Clock* clock);
  virtual ~DelayBasedBwe() {}

  void IncomingPacketFeedbackVector(
      const std::vector<PacketInfo>& packet_feedback_vector) override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const override;
  void SetMinBitrate(int min_bitrate_bps) override;

  // Required by RemoteBitrateEstimator but does nothing.
  void Process() override;
  // Required by RemoteBitrateEstimator but does nothing.
  int64_t TimeUntilNextProcess() override;
  // Required by RemoteBitrateEstimator but does nothing.
  void RemoveStream(uint32_t ssrc) override;
  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override {
    RTC_NOTREACHED();
  }

 private:
  void IncomingPacketInfo(const PacketInfo& info);
  // Updates the current remote rate estimate and returns true if a valid
  // estimate exists.
  bool UpdateEstimate(int64_t packet_arrival_time_ms,
                      int64_t now_ms,
                      uint32_t* target_bitrate_bps)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::ThreadChecker network_thread_;
  Clock* const clock_;
  RemoteBitrateObserver* const observer_;
  std::unique_ptr<InterArrival> inter_arrival_;
  std::unique_ptr<OveruseEstimator> estimator_;
  OveruseDetector detector_;
  RateStatistics incoming_bitrate_;
  int64_t last_update_ms_;
  int64_t last_seen_packet_ms_;
  bool uma_recorded_;

  rtc::CriticalSection crit_;
  AimdRateControl remote_rate_ GUARDED_BY(&crit_);
  ProbeBitrateEstimator probe_bitrate_estimator_ GUARDED_BY(&crit_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayBasedBwe);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_
