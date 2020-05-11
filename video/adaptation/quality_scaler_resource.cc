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

QualityScalerResource::QualityScalerResource(
    ResourceAdaptationProcessorInterface* adaptation_processor)
    : adaptation_processor_(adaptation_processor),
      quality_scaler_(nullptr),
      pending_qp_usage_callback_(nullptr) {}

bool QualityScalerResource::is_started() const {
  return quality_scaler_.get();
}

void QualityScalerResource::StartCheckForOveruse(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK(!is_started());
  quality_scaler_ =
      std::make_unique<QualityScaler>(this, std::move(qp_thresholds));
}

void QualityScalerResource::StopCheckForOveruse() {
  quality_scaler_.reset();
}

void QualityScalerResource::SetQpThresholds(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK(is_started());
  quality_scaler_->SetQpThresholds(std::move(qp_thresholds));
}

bool QualityScalerResource::QpFastFilterLow() {
  RTC_DCHECK(is_started());
  return quality_scaler_->QpFastFilterLow();
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
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
  RTC_DCHECK(!pending_qp_usage_callback_);
  pending_qp_usage_callback_ = std::move(callback);
  // If this triggers adaptation, OnAdaptationApplied() is called by the
  // processor where we determine if QP should be cleared and we invoke and null
  // the |pending_qp_usage_callback_|.
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  // If |pending_qp_usage_callback_| has not been nulled yet then we did not
  // just trigger an adaptation and should not clear the QP samples.
  if (pending_qp_usage_callback_) {
    pending_qp_usage_callback_->OnQpUsageHandled(false);
    pending_qp_usage_callback_ = nullptr;
  }
}

void QualityScalerResource::OnReportQpUsageLow(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback) {
  RTC_DCHECK(!pending_qp_usage_callback_);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
  callback->OnQpUsageHandled(true);
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {
  // We only clear QP samples on adaptations triggered by the QualityScaler.
  if (!pending_qp_usage_callback_)
    return;
  bool clear_qp_samples = true;
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
  if (adaptation_processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      DidDecreaseFrameRate(restrictions_before, restrictions_after)) {
    absl::optional<int> min_diff = BalancedDegradationSettings().MinFpsDiff(
        input_state.frame_size_pixels().value());
    if (min_diff && input_state.frames_per_second() > 0) {
      int fps_diff = input_state.frames_per_second() -
                     restrictions_after.max_frame_rate().value();
      if (fps_diff < min_diff.value()) {
        clear_qp_samples = false;
      }
    }
  }
  pending_qp_usage_callback_->OnQpUsageHandled(clear_qp_samples);
  pending_qp_usage_callback_ = nullptr;
}

}  // namespace webrtc
