/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/quality_scaler_resource.h"

#include <utility>

#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {

const int64_t kUnderuseDueToDisabledCooldownMs = 1000;

}  // namespace

// static
rtc::scoped_refptr<QualityScalerResource> QualityScalerResource::Create(
    DegradationPreferenceProvider* degradation_preference_provider) {
  return new rtc::RefCountedObject<QualityScalerResource>(
      degradation_preference_provider);
}

QualityScalerResource::QualityScalerResource(
    DegradationPreferenceProvider* degradation_preference_provider)
    : VideoStreamEncoderResource("QualityScalerResource"),
      quality_scaler_(nullptr),
      last_underuse_due_to_disabled_timestamp_ms_(absl::nullopt),
      num_handled_callbacks_(0),
      pending_callbacks_(),
      degradation_preference_provider_(degradation_preference_provider),
      clear_qp_samples_(false) {
  RTC_CHECK(degradation_preference_provider_);
}

QualityScalerResource::~QualityScalerResource() {
  RTC_DCHECK(!quality_scaler_);
  RTC_DCHECK(pending_callbacks_.empty());
}

bool QualityScalerResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue());
  return quality_scaler_.get();
}

void QualityScalerResource::StartCheckForOveruse(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(!is_started());
  quality_scaler_ =
      std::make_unique<QualityScaler>(this, std::move(qp_thresholds));
}

void QualityScalerResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  // Ensure we have no pending callbacks. This makes it safe to destroy the
  // QualityScaler and even task queues with tasks in-flight.
  AbortPendingCallbacks();
  quality_scaler_.reset();
}

void QualityScalerResource::SetQpThresholds(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  quality_scaler_->SetQpThresholds(std::move(qp_thresholds));
}

bool QualityScalerResource::QpFastFilterLow() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  return quality_scaler_->QpFastFilterLow();
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  if (quality_scaler_ && encoded_image.qp_ >= 0) {
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_in_us);
  } else if (!quality_scaler_) {
    // Reference counting guarantees that this object is still alive by the time
    // the task is executed.
    // TODO(webrtc:11553): this is a workaround to ensure that all quality
    // scaler imposed limitations are removed once qualty scaler is disabled
    // mid call.
    // Instead it should be done at a higher layer in the same way for all
    // resources.
    int64_t timestamp_ms = rtc::TimeMillis();
    if (!last_underuse_due_to_disabled_timestamp_ms_.has_value() ||
        timestamp_ms - last_underuse_due_to_disabled_timestamp_ms_.value() >=
            kUnderuseDueToDisabledCooldownMs) {
      last_underuse_due_to_disabled_timestamp_ms_ = timestamp_ms;
      MaybePostTaskToResourceAdaptationQueue(
          [this_ref = rtc::scoped_refptr<QualityScalerResource>(this)] {
            RTC_DCHECK_RUN_ON(this_ref->resource_adaptation_queue());
            this_ref->OnResourceUsageStateMeasured(
                ResourceUsageState::kUnderuse);
          });
    }
  }
}

void QualityScalerResource::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  if (!quality_scaler_)
    return;
  switch (reason) {
    case EncodedImageCallback::DropReason::kDroppedByMediaOptimizations:
      quality_scaler_->ReportDroppedFrameByMediaOpt();
      break;
    case EncodedImageCallback::DropReason::kDroppedByEncoder:
      quality_scaler_->ReportDroppedFrameByEncoder();
      break;
  }
}

void QualityScalerResource::OnReportQpUsageHigh(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  size_t callback_id = QueuePendingCallback(callback);
  // Reference counting guarantees that this object is still alive by the time
  // the task is executed.
  MaybePostTaskToResourceAdaptationQueue(
      [this_ref = rtc::scoped_refptr<QualityScalerResource>(this),
       callback_id] {
        RTC_DCHECK_RUN_ON(this_ref->resource_adaptation_queue());
        this_ref->clear_qp_samples_ = false;
        // If this OnResourceUsageStateMeasured() triggers an adaptation,
        // OnAdaptationApplied() will occur between this line and the next. This
        // allows modifying |clear_qp_samples_| based on the adaptation.
        this_ref->OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
        this_ref->HandlePendingCallback(callback_id,
                                        this_ref->clear_qp_samples_);
      });
}

void QualityScalerResource::OnReportQpUsageLow(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  size_t callback_id = QueuePendingCallback(callback);
  // Reference counting guarantees that this object is still alive by the time
  // the task is executed.
  MaybePostTaskToResourceAdaptationQueue(
      [this_ref = rtc::scoped_refptr<QualityScalerResource>(this),
       callback_id] {
        RTC_DCHECK_RUN_ON(this_ref->resource_adaptation_queue());
        this_ref->OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
        this_ref->HandlePendingCallback(callback_id, true);
      });
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue());
  // We only clear QP samples on adaptations triggered by the QualityScaler.
  if (reason_resource != this)
    return;
  clear_qp_samples_ = true;
  // If we're in "balanced" and the frame rate before and after adaptation did
  // not differ that much, don't clear the QP samples and instead check for QP
  // again in a short amount of time. This may trigger adapting down again soon.
  // TODO(hbos): Can this be simplified by getting rid of special casing logic?
  // For example, we could decide whether or not to clear QP samples based on
  // how big the adaptation step was alone (regardless of degradation preference
  // or what resource triggered the adaptation) and the QualityScaler could
  // check for QP when it had enough QP samples rather than at a variable
  // interval whose delay is calculated based on events such as these. Now there
  // is much dependency on a specific OnReportQpUsageHigh() event and "balanced"
  // but adaptations happening might not align with QualityScaler's CheckQpTask.
  if (degradation_preference_provider_->degradation_preference() ==
          DegradationPreference::BALANCED &&
      DidDecreaseFrameRate(restrictions_before, restrictions_after)) {
    absl::optional<int> min_diff = BalancedDegradationSettings().MinFpsDiff(
        input_state.frame_size_pixels().value());
    if (min_diff && input_state.frames_per_second() > 0) {
      int fps_diff = input_state.frames_per_second() -
                     restrictions_after.max_frame_rate().value();
      if (fps_diff < min_diff.value()) {
        clear_qp_samples_ = false;
      }
    }
  }
}

size_t QualityScalerResource::QueuePendingCallback(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  pending_callbacks_.push(callback);
  // The ID of a callback is its sequence number (1, 2, 3...).
  return num_handled_callbacks_ + pending_callbacks_.size();
}

void QualityScalerResource::HandlePendingCallback(size_t callback_id,
                                                  bool clear_qp_samples) {
  RTC_DCHECK_RUN_ON(resource_adaptation_queue());
  // Reference counting guarantees that this object is still alive by the time
  // the task is executed.
  encoder_queue()->PostTask(
      ToQueuedTask([this_ref = rtc::scoped_refptr<QualityScalerResource>(this),
                    callback_id, clear_qp_samples] {
        RTC_DCHECK_RUN_ON(this_ref->encoder_queue());
        if (this_ref->num_handled_callbacks_ >= callback_id) {
          // The callback with this ID has already been handled.
          // This happens if AbortPendingCallbacks() is called while the task is
          // in flight.
          return;
        }
        RTC_DCHECK(!this_ref->pending_callbacks_.empty());
        this_ref->pending_callbacks_.front()->OnQpUsageHandled(
            clear_qp_samples);
        ++this_ref->num_handled_callbacks_;
        this_ref->pending_callbacks_.pop();
      }));
}

void QualityScalerResource::AbortPendingCallbacks() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  while (!pending_callbacks_.empty()) {
    pending_callbacks_.front()->OnQpUsageHandled(false);
    ++num_handled_callbacks_;
    pending_callbacks_.pop();
  }
}

}  // namespace webrtc
