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

#include <memory>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using testing::_;
using testing::Eq;
using testing::StrictMock;

namespace {

class FakeDegradationPreferenceProvider : public DegradationPreferenceProvider {
 public:
  ~FakeDegradationPreferenceProvider() override = default;

  DegradationPreference degradation_preference() const override {
    return DegradationPreference::MAINTAIN_FRAMERATE;
  }
};

}  // namespace

class QualityScalerResourceTest : public ::testing::Test {
 public:
  QualityScalerResourceTest()
      : resource_adaptation_queue_("ResourceAdaptationQueue",
                                   TaskQueueFactory::Priority::NORMAL),
        encoder_queue_("EncoderQueue", TaskQueueFactory::Priority::NORMAL),
        quality_scaler_resource_(
            QualityScalerResource::Create(&degradation_preference_provider_)) {
    quality_scaler_resource_->RegisterEncoderTaskQueue(encoder_queue_.Get());
    quality_scaler_resource_->RegisterAdaptationTaskQueue(
        resource_adaptation_queue_.Get());
    encoder_queue_.SendTask(
        [this] {
          quality_scaler_resource_->SetResourceListener(
              &fake_resource_listener_);
          quality_scaler_resource_->StartCheckForOveruse(
              VideoEncoder::QpThresholds());
        },
        RTC_FROM_HERE);
  }

  ~QualityScalerResourceTest() override {
    encoder_queue_.SendTask(
        [this] {
          quality_scaler_resource_->StopCheckForOveruse();
          quality_scaler_resource_->SetResourceListener(nullptr);
        },
        RTC_FROM_HERE);
  }

 protected:
  TaskQueueForTest resource_adaptation_queue_;
  TaskQueueForTest encoder_queue_;
  StrictMock<MockResourceListener> fake_resource_listener_;
  FakeDegradationPreferenceProvider degradation_preference_provider_;
  rtc::scoped_refptr<QualityScalerResource> quality_scaler_resource_;
};

TEST_F(QualityScalerResourceTest, ReportQpHigh) {
  EXPECT_CALL(fake_resource_listener_,
              OnResourceUsageStateMeasured(Eq(quality_scaler_resource_),
                                           Eq(ResourceUsageState::kOveruse)));
  encoder_queue_.SendTask(
      [this] { quality_scaler_resource_->OnReportQpUsageHigh(); },
      RTC_FROM_HERE);
  // Wait for adapt queue to clear since that signals the resource listener.
  resource_adaptation_queue_.WaitForPreviouslyPostedTasks();
}

TEST_F(QualityScalerResourceTest, ReportQpLow) {
  EXPECT_CALL(fake_resource_listener_,
              OnResourceUsageStateMeasured(Eq(quality_scaler_resource_),
                                           Eq(ResourceUsageState::kUnderuse)));
  encoder_queue_.SendTask(
      [this] { quality_scaler_resource_->OnReportQpUsageLow(); },
      RTC_FROM_HERE);
  // Wait for adapt queue to clear since that signals the resource listener.
  resource_adaptation_queue_.WaitForPreviouslyPostedTasks();
}

}  // namespace webrtc
