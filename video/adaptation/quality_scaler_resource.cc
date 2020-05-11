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

namespace webrtc {

QualityScalerResource::QualityScalerResource()
    : rtc::RefCountedObject<Resource>(),
      encoder_queue_(nullptr),
      adaptation_processor_(nullptr),
      quality_scaler_(nullptr),
      num_handled_callbacks_(0),
      pending_callbacks_(),
      processing_in_progress_(false),
      clear_qp_samples_(false) {}

QualityScalerResource::~QualityScalerResource() {
  RTC_DCHECK(!quality_scaler_);
  RTC_DCHECK(pending_callbacks_.empty());
}

void QualityScalerResource::Initialize(rtc::TaskQueue* encoder_queue) {
  RTC_DCHECK(!encoder_queue_);
  RTC_DCHECK(encoder_queue);
  encoder_queue_ = encoder_queue;
}

void QualityScalerResource::SetAdaptationProcessor(
    ResourceAdaptationProcessorInterface* adaptation_processor) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  adaptation_processor_ = adaptation_processor;
}

bool QualityScalerResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  return quality_scaler_.get();
}

void QualityScalerResource::StartCheckForOveruse(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(!is_started());
  quality_scaler_ =
      std::make_unique<QualityScaler>(this, std::move(qp_thresholds));
}

void QualityScalerResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // Ensure we have no pending callbacks. This makes it safe to destroy the
  // QualityScaler and even task queues with tasks in-flight.
  AbortPendingCallbacks();
  quality_scaler_.reset();
}

void QualityScalerResource::SetQpThresholds(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(is_started());
  quality_scaler_->SetQpThresholds(std::move(qp_thresholds));
}

bool QualityScalerResource::QpFastFilterLow() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(is_started());
  return quality_scaler_->QpFastFilterLow();
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  if (quality_scaler_ && encoded_image.qp_ >= 0) {
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_in_us);
  } else if (!quality_scaler_) {
    // TODO(webrtc:11553): this is a workaround to ensure that all quality
    // scaler imposed limitations are removed once qualty scaler is disabled
    // mid call.
    // Instead it should be done at a higher layer in the same way for all
    // resources.
    OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
  }
}

void QualityScalerResource::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
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
  RTC_DCHECK_RUN_ON(encoder_queue_);
  size_t callback_id = QueuePendingCallback(callback);
  // TODO(https://crbug.com/webrtc/11542): When we have an adaptation queue,
  // PostTask the resource usage measurements.
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  clear_qp_samples_ = false;
  // If this OnResourceUsageStateMeasured() triggers an adaptation,
  // OnAdaptationApplied() will occur between this line and the next. This
  // allows modifying |clear_qp_samples_| based on the adaptation.
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  HandlePendingCallback(callback_id, clear_qp_samples_);
  processing_in_progress_ = false;
}

void QualityScalerResource::OnReportQpUsageLow(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  size_t callback_id = QueuePendingCallback(callback);
  // TODO(https://crbug.com/webrtc/11542): When we have an adaptation queue,
  // PostTask the resource usage measurements.
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
  HandlePendingCallback(callback_id, true);
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // We only clear QP samples on adaptations triggered by the QualityScaler.
  if (!processing_in_progress_)
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
  if (adaptation_processor_ &&
      adaptation_processor_->effective_degradation_preference() ==
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
  RTC_DCHECK_RUN_ON(encoder_queue_);
  pending_callbacks_.push(callback);
  // The ID of a callback is its sequence number (1, 2, 3...).
  return num_handled_callbacks_ + pending_callbacks_.size();
}

void QualityScalerResource::HandlePendingCallback(size_t callback_id,
                                                  bool clear_qp_samples) {
  // TODO(https://crbug.com/webrtc/11542): When we have an adaptation queue,
  // this method would be invoked on the adaptation queue and a PostTask would
  // be used to resolve the callback.
  RTC_DCHECK_RUN_ON(encoder_queue_);
  if (num_handled_callbacks_ >= callback_id) {
    // The callback with this ID has already been handled.
    // This happens if AbortPendingCallbacks() is called while the task is
    // in flight.
    return;
  }
  RTC_DCHECK(!pending_callbacks_.empty());
  pending_callbacks_.front()->OnQpUsageHandled(clear_qp_samples);
  ++num_handled_callbacks_;
  pending_callbacks_.pop();
}

void QualityScalerResource::AbortPendingCallbacks() {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  while (!pending_callbacks_.empty()) {
    pending_callbacks_.front()->OnQpUsageHandled(false);
    ++num_handled_callbacks_;
    pending_callbacks_.pop();
  }
}

}  // namespace webrtc
