/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/delay_based_bwe.h"

#include <math.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/typedefs.h"

namespace {
constexpr int kTimestampGroupLengthMs = 5;
constexpr int kAbsSendTimeFraction = 18;
constexpr int kAbsSendTimeInterArrivalUpshift = 8;
constexpr int kInterArrivalShift =
    kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;
constexpr double kTimestampToMs =
    1000.0 / static_cast<double>(1 << kInterArrivalShift);
// This ssrc is used to fulfill the current API but will be removed
// after the API has been changed.
constexpr uint32_t kFixedSsrc = 0;
}  // namespace

namespace webrtc {

DelayBasedBwe::DelayBasedBwe(RemoteBitrateObserver* observer, Clock* clock)
    : clock_(clock),
      observer_(observer),
      inter_arrival_(),
      estimator_(),
      detector_(OverUseDetectorOptions()),
      incoming_bitrate_(kBitrateWindowMs, 8000),
      last_update_ms_(-1),
      last_seen_packet_ms_(-1),
      uma_recorded_(false) {
  RTC_DCHECK(observer_);
  network_thread_.DetachFromThread();
}

void DelayBasedBwe::IncomingPacketFeedbackVector(
    const std::vector<PacketInfo>& packet_feedback_vector) {
  RTC_DCHECK(network_thread_.CalledOnValidThread());
  if (!uma_recorded_) {
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram,
                              BweNames::kSendSideTransportSeqNum,
                              BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  for (const auto& packet_info : packet_feedback_vector) {
    IncomingPacketInfo(packet_info);
  }
}

void DelayBasedBwe::IncomingPacketInfo(const PacketInfo& info) {
  int64_t now_ms = clock_->TimeInMilliseconds();

  incoming_bitrate_.Update(info.payload_size, info.arrival_time_ms);
  bool delay_based_bwe_changed = false;
  uint32_t target_bitrate_bps = 0;
  {
    rtc::CritScope lock(&crit_);

    // Reset if the stream has timed out.
    if (last_seen_packet_ms_ == -1 ||
        now_ms - last_seen_packet_ms_ > kStreamTimeOutMs) {
      inter_arrival_.reset(new InterArrival(
          (kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
          kTimestampToMs, true));
      estimator_.reset(new OveruseEstimator(OverUseDetectorOptions()));
    }
    last_seen_packet_ms_ = now_ms;

    uint32_t send_time_24bits =
        static_cast<uint32_t>(((static_cast<uint64_t>(info.send_time_ms)
                                << kAbsSendTimeFraction) +
                               500) /
                              1000) &
        0x00FFFFFF;
    // Shift up send time to use the full 32 bits that inter_arrival works with,
    // so wrapping works properly.
    uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;

    uint32_t ts_delta = 0;
    int64_t t_delta = 0;
    int size_delta = 0;
    if (inter_arrival_->ComputeDeltas(timestamp, info.arrival_time_ms, now_ms,
                                      info.payload_size, &ts_delta, &t_delta,
                                      &size_delta)) {
      double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
      estimator_->Update(t_delta, ts_delta_ms, size_delta, detector_.State(),
                         info.arrival_time_ms);
      detector_.Detect(estimator_->offset(), ts_delta_ms,
                       estimator_->num_of_deltas(), info.arrival_time_ms);
    }

    int probing_bps = 0;
    if (info.probe_cluster_id != PacketInfo::kNotAProbe) {
      probing_bps =
          probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(info);
    }

    // Currently overusing the bandwidth.
    if (detector_.State() == kBwOverusing) {
      rtc::Optional<uint32_t> incoming_rate =
          incoming_bitrate_.Rate(info.arrival_time_ms);
      if (incoming_rate &&
          remote_rate_.TimeToReduceFurther(now_ms, *incoming_rate)) {
        delay_based_bwe_changed =
            UpdateEstimate(info.arrival_time_ms, now_ms, &target_bitrate_bps);
      }
    } else if (probing_bps > 0) {
      // No overuse, but probing measured a bitrate.
      remote_rate_.SetEstimate(probing_bps, info.arrival_time_ms);
      observer_->OnProbeBitrate(probing_bps);
      delay_based_bwe_changed =
          UpdateEstimate(info.arrival_time_ms, now_ms, &target_bitrate_bps);
    }
    if (!delay_based_bwe_changed &&
        (last_update_ms_ == -1 ||
         now_ms - last_update_ms_ > remote_rate_.GetFeedbackInterval())) {
      delay_based_bwe_changed =
          UpdateEstimate(info.arrival_time_ms, now_ms, &target_bitrate_bps);
    }
  }

  if (delay_based_bwe_changed) {
    last_update_ms_ = now_ms;
    observer_->OnReceiveBitrateChanged({kFixedSsrc}, target_bitrate_bps);
  }
}

bool DelayBasedBwe::UpdateEstimate(int64_t arrival_time_ms,
                                   int64_t now_ms,
                                   uint32_t* target_bitrate_bps) {
  // The first overuse should immediately trigger a new estimate.
  // We also have to update the estimate immediately if we are overusing
  // and the target bitrate is too high compared to what we are receiving.
  const RateControlInput input(detector_.State(),
                               incoming_bitrate_.Rate(arrival_time_ms),
                               estimator_->var_noise());
  remote_rate_.Update(&input, now_ms);
  *target_bitrate_bps = remote_rate_.UpdateBandwidthEstimate(now_ms);
  return remote_rate_.ValidEstimate();
}

void DelayBasedBwe::Process() {}

int64_t DelayBasedBwe::TimeUntilNextProcess() {
  const int64_t kDisabledModuleTime = 1000;
  return kDisabledModuleTime;
}

void DelayBasedBwe::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  rtc::CritScope lock(&crit_);
  remote_rate_.SetRtt(avg_rtt_ms);
}

void DelayBasedBwe::RemoveStream(uint32_t ssrc) {}

bool DelayBasedBwe::LatestEstimate(std::vector<uint32_t>* ssrcs,
                                   uint32_t* bitrate_bps) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  RTC_DCHECK(ssrcs);
  RTC_DCHECK(bitrate_bps);
  rtc::CritScope lock(&crit_);
  if (!remote_rate_.ValidEstimate())
    return false;

  *ssrcs = {kFixedSsrc};
  *bitrate_bps = remote_rate_.LatestEstimate();
  return true;
}

void DelayBasedBwe::SetMinBitrate(int min_bitrate_bps) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rtc::CritScope lock(&crit_);
  remote_rate_.SetMinBitrate(min_bitrate_bps);
}
}  // namespace webrtc
