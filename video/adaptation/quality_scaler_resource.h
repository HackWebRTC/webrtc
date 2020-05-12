/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
#define VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_

#include <memory>
#include <queue>
#include <string>

#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

// Handles interaction with the QualityScaler.
class QualityScalerResource : public rtc::RefCountedObject<Resource>,
                              public QualityScalerQpUsageHandlerInterface {
 public:
  QualityScalerResource();
  ~QualityScalerResource() override;

  void SetAdaptationProcessor(
      ResourceAdaptationProcessorInterface* adaptation_processor);

  bool is_started() const;

  void StartCheckForOveruse(VideoEncoder::QpThresholds qp_thresholds);
  void StopCheckForOveruse();

  void SetQpThresholds(VideoEncoder::QpThresholds qp_thresholds);
  bool QpFastFilterLow();
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us);
  void OnFrameDropped(EncodedImageCallback::DropReason reason);

  // QualityScalerQpUsageHandlerInterface implementation.
  void OnReportQpUsageHigh(
      rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback)
      override;
  void OnReportQpUsageLow(
      rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface> callback)
      override;

  std::string name() const override { return "QualityScalerResource"; }

  // Resource implementation.
  void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) override;

 private:
  size_t QueuePendingCallback(
      rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface>
          callback);
  void HandlePendingCallback(size_t callback_id, bool clear_qp_samples);
  void AbortPendingCallbacks();

  // Members accessed on the encoder queue.
  std::unique_ptr<QualityScaler> quality_scaler_
      RTC_GUARDED_BY(encoder_queue());
  // Every OnReportQpUsageHigh/Low() operation has a callback that MUST be
  // invoked on the |encoder_queue_|. Because usage measurements are reported on
  // the |encoder_queue_| but handled by the processor on the the
  // |resource_adaptation_queue_|, handling a measurement entails a task queue
  // "ping" round-trip. Multiple callbacks in-flight is thus possible.
  size_t num_handled_callbacks_ RTC_GUARDED_BY(encoder_queue());
  std::queue<rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface>>
      pending_callbacks_ RTC_GUARDED_BY(encoder_queue());

  // Members accessed on the adaptation queue.
  ResourceAdaptationProcessorInterface* adaptation_processor_
      RTC_GUARDED_BY(resource_adaptation_queue());
  bool clear_qp_samples_ RTC_GUARDED_BY(resource_adaptation_queue());
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
