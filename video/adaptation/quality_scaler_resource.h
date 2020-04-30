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
#include <string>

#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "modules/video_coding/utility/quality_scaler.h"

namespace webrtc {

// Handles interaction with the QualityScaler.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly by usage in the ResourceAdaptationProcessor (which is only tested
// because of its usage in VideoStreamEncoder); all tests are currently in
// video_stream_encoder_unittest.cc.
class QualityScalerResource : public Resource,
                              public QualityScalerQpUsageHandlerInterface {
 public:
  explicit QualityScalerResource(
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
  void OnAdaptationApplied(const VideoStreamInputState& input_state,
                           const VideoSourceRestrictions& restrictions_before,
                           const VideoSourceRestrictions& restrictions_after,
                           const Resource& reason_resource) override;

 private:
  ResourceAdaptationProcessorInterface* const adaptation_processor_;
  std::unique_ptr<QualityScaler> quality_scaler_;
  rtc::scoped_refptr<QualityScalerQpUsageHandlerCallbackInterface>
      pending_qp_usage_callback_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
