/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/encode_usage_resource.h"

#include <limits>
#include <utility>

#include "api/video/video_adaptation_reason.h"
#include "rtc_base/checks.h"

namespace webrtc {

EncodeUsageResource::EncodeUsageResource(
    std::unique_ptr<OveruseFrameDetector> overuse_detector)
    : rtc::RefCountedObject<Resource>(),
      encoder_queue_(nullptr),
      overuse_detector_(std::move(overuse_detector)),
      is_started_(false),
      target_frame_rate_(absl::nullopt) {
  RTC_DCHECK(overuse_detector_);
}

void EncodeUsageResource::Initialize(rtc::TaskQueue* encoder_queue) {
  RTC_DCHECK(!encoder_queue_);
  RTC_DCHECK(encoder_queue);
  encoder_queue_ = encoder_queue;
}

bool EncodeUsageResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  return is_started_;
}

void EncodeUsageResource::StartCheckForOveruse(CpuOveruseOptions options) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(!is_started_);
  overuse_detector_->StartCheckForOveruse(TaskQueueBase::Current(),
                                          std::move(options), this);
  is_started_ = true;
  overuse_detector_->OnTargetFramerateUpdated(TargetFrameRateAsInt());
}

void EncodeUsageResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  overuse_detector_->StopCheckForOveruse();
  is_started_ = false;
}

void EncodeUsageResource::SetTargetFrameRate(
    absl::optional<double> target_frame_rate) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  if (target_frame_rate == target_frame_rate_)
    return;
  target_frame_rate_ = target_frame_rate;
  if (is_started_)
    overuse_detector_->OnTargetFramerateUpdated(TargetFrameRateAsInt());
}

void EncodeUsageResource::OnEncodeStarted(const VideoFrame& cropped_frame,
                                          int64_t time_when_first_seen_us) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // TODO(hbos): Rename FrameCaptured() to something more appropriate (e.g.
  // "OnEncodeStarted"?) or revise usage.
  overuse_detector_->FrameCaptured(cropped_frame, time_when_first_seen_us);
}

void EncodeUsageResource::OnEncodeCompleted(
    uint32_t timestamp,
    int64_t time_sent_in_us,
    int64_t capture_time_us,
    absl::optional<int> encode_duration_us) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // TODO(hbos): Rename FrameSent() to something more appropriate (e.g.
  // "OnEncodeCompleted"?).
  overuse_detector_->FrameSent(timestamp, time_sent_in_us, capture_time_us,
                               encode_duration_us);
}

void EncodeUsageResource::AdaptUp() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // TODO(https://crbug.com/webrtc/11542): When we have an adaptation queue,
  // PostTask the resource usage measurements.
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

void EncodeUsageResource::AdaptDown() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // TODO(https://crbug.com/webrtc/11542): When we have an adaptation queue,
  // PostTask the resource usage measurements.
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
}

int EncodeUsageResource::TargetFrameRateAsInt() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  return target_frame_rate_.has_value()
             ? static_cast<int>(target_frame_rate_.value())
             : std::numeric_limits<int>::max();
}

}  // namespace webrtc
