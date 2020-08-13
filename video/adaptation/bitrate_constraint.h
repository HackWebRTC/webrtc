/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_BITRATE_CONSTRAINT_H_
#define VIDEO_ADAPTATION_BITRATE_CONSTRAINT_H_

#include <string>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"

namespace webrtc {

class BitrateConstraint : public rtc::RefCountInterface,
                          public AdaptationConstraint {
 public:
  BitrateConstraint();
  ~BitrateConstraint() override = default;

  void SetAdaptationQueue(TaskQueueBase* resource_adaptation_queue);
  void OnEncoderSettingsUpdated(
      absl::optional<EncoderSettings> encoder_settings);
  void OnEncoderTargetBitrateUpdated(
      absl::optional<uint32_t> encoder_target_bitrate_bps);

  // AdaptationConstraint implementation.
  std::string Name() const override { return "BitrateConstraint"; }
  bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after) const override;

 private:
  // The |manager_| must be alive as long as this resource is added to the
  // ResourceAdaptationProcessor, i.e. when IsAdaptationUpAllowed() is called.
  TaskQueueBase* resource_adaptation_queue_;
  absl::optional<EncoderSettings> encoder_settings_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  absl::optional<uint32_t> encoder_target_bitrate_bps_
      RTC_GUARDED_BY(resource_adaptation_queue_);
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_BITRATE_CONSTRAINT_H_
