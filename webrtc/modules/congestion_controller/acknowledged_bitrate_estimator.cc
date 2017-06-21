/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/acknowledged_bitrate_estimator.h"

#include <utility>

#include "webrtc/base/ptr_util.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

namespace {
bool IsInSendTimeHistory(const PacketFeedback& packet) {
  return packet.send_time_ms >= 0;
}
}  // namespace

std::unique_ptr<BitrateEstimator> BitrateEstimatorCreator::Create() {
  return rtc::MakeUnique<BitrateEstimator>();
}

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator()
    : AcknowledgedBitrateEstimator(rtc::MakeUnique<BitrateEstimatorCreator>()) {
}

AcknowledgedBitrateEstimator::AcknowledgedBitrateEstimator(
    std::unique_ptr<BitrateEstimatorCreator> bitrate_estimator_creator)
    : was_in_alr_(false),
      bitrate_estimator_creator_(std::move(bitrate_estimator_creator)),
      bitrate_estimator_(bitrate_estimator_creator_->Create()) {}

void AcknowledgedBitrateEstimator::IncomingPacketFeedbackVector(
    const std::vector<PacketFeedback>& packet_feedback_vector,
    bool currently_in_alr) {
  RTC_DCHECK(std::is_sorted(packet_feedback_vector.begin(),
                            packet_feedback_vector.end(),
                            PacketFeedbackComparator()));
  MaybeResetBitrateEstimator(currently_in_alr);
  for (const auto& packet : packet_feedback_vector) {
    if (IsInSendTimeHistory(packet) && !SentBeforeAlrEnded(packet))
      bitrate_estimator_->Update(packet.arrival_time_ms, packet.payload_size);
  }
}

rtc::Optional<uint32_t> AcknowledgedBitrateEstimator::bitrate_bps() const {
  return bitrate_estimator_->bitrate_bps();
}

bool AcknowledgedBitrateEstimator::SentBeforeAlrEnded(
    const PacketFeedback& packet) {
  if (alr_ended_time_ms_) {
    if (*alr_ended_time_ms_ > packet.send_time_ms) {
      return true;
    }
  }
  return false;
}

bool AcknowledgedBitrateEstimator::AlrEnded(bool currently_in_alr) const {
  return was_in_alr_ && !currently_in_alr;
}

void AcknowledgedBitrateEstimator::MaybeResetBitrateEstimator(
    bool currently_in_alr) {
  if (AlrEnded(currently_in_alr)) {
    bitrate_estimator_ = bitrate_estimator_creator_->Create();
    alr_ended_time_ms_ = rtc::Optional<int64_t>(rtc::TimeMillis());
  }
  was_in_alr_ = currently_in_alr;
}

}  // namespace webrtc
