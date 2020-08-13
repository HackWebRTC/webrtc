/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "rtc_base/task_utils/to_queued_task.h"
#include "video/adaptation/balanced_constraint.h"

namespace webrtc {

BalancedConstraint::BalancedConstraint(
    DegradationPreferenceProvider* degradation_preference_provider)
    : resource_adaptation_queue_(nullptr),
      encoder_target_bitrate_bps_(absl::nullopt),
      degradation_preference_provider_(degradation_preference_provider) {
  RTC_DCHECK(degradation_preference_provider_);
}

void BalancedConstraint::SetAdaptationQueue(
    TaskQueueBase* resource_adaptation_queue) {
  resource_adaptation_queue_ = resource_adaptation_queue;
}

void BalancedConstraint::OnEncoderTargetBitrateUpdated(
    absl::optional<uint32_t> encoder_target_bitrate_bps) {
  resource_adaptation_queue_->PostTask(
      ToQueuedTask([this_ref = rtc::scoped_refptr<BalancedConstraint>(this),
                    encoder_target_bitrate_bps] {
        RTC_DCHECK_RUN_ON(this_ref->resource_adaptation_queue_);
        this_ref->encoder_target_bitrate_bps_ = encoder_target_bitrate_bps;
      }));
}

bool BalancedConstraint::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after) const {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue_);
  // Don't adapt if BalancedDegradationSettings applies and determines this will
  // exceed bitrate constraints.
  if (degradation_preference_provider_->degradation_preference() ==
          DegradationPreference::BALANCED &&
      !balanced_settings_.CanAdaptUp(input_state.video_codec_type(),
                                     input_state.frame_size_pixels().value(),
                                     encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  if (DidIncreaseResolution(restrictions_before, restrictions_after) &&
      !balanced_settings_.CanAdaptUpResolution(
          input_state.video_codec_type(),
          input_state.frame_size_pixels().value(),
          encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  return true;
}

}  // namespace webrtc
