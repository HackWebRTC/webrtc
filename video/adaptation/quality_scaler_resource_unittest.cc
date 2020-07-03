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
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const int kDefaultTimeout = 5000;

class FakeQualityScalerQpUsageHandlerCallback
    : public QualityScalerQpUsageHandlerCallbackInterface {
 public:
  explicit FakeQualityScalerQpUsageHandlerCallback(
      rtc::TaskQueue* encoder_queue)
      : QualityScalerQpUsageHandlerCallbackInterface(),
        encoder_queue_(encoder_queue),
        is_handled_(false),
        qp_usage_handled_event_(true /* manual_reset */, false),
        clear_qp_samples_result_(absl::nullopt) {}
  ~FakeQualityScalerQpUsageHandlerCallback() override {
    RTC_DCHECK(is_handled_)
        << "The callback was destroyed without being invoked.";
  }

  void OnQpUsageHandled(bool clear_qp_samples) override {
    ASSERT_TRUE(encoder_queue_->IsCurrent());
    RTC_DCHECK(!is_handled_);
    clear_qp_samples_result_ = clear_qp_samples;
    is_handled_ = true;
    qp_usage_handled_event_.Set();
  }

  bool is_handled() const { return is_handled_; }
  rtc::Event* qp_usage_handled_event() { return &qp_usage_handled_event_; }
  absl::optional<bool> clear_qp_samples_result() const {
    return clear_qp_samples_result_;
  }

 private:
  rtc::TaskQueue* const encoder_queue_;
  bool is_handled_;
  rtc::Event qp_usage_handled_event_;
  absl::optional<bool> clear_qp_samples_result_;
};

class FakeDegradationPreferenceProvider : public DegradationPreferenceProvider {
 public:
  ~FakeDegradationPreferenceProvider() override {}

  DegradationPreference degradation_preference() const override {
    return DegradationPreference::MAINTAIN_FRAMERATE;
  }
};

}  // namespace

class QualityScalerResourceTest : public ::testing::Test {
 public:
  QualityScalerResourceTest()
      : task_queue_factory_(CreateDefaultTaskQueueFactory()),
        resource_adaptation_queue_(task_queue_factory_->CreateTaskQueue(
            "ResourceAdaptationQueue",
            TaskQueueFactory::Priority::NORMAL)),
        encoder_queue_(task_queue_factory_->CreateTaskQueue(
            "EncoderQueue",
            TaskQueueFactory::Priority::NORMAL)),
        degradation_preference_provider_(),
        quality_scaler_resource_(
            QualityScalerResource::Create(&degradation_preference_provider_)) {
    quality_scaler_resource_->RegisterEncoderTaskQueue(encoder_queue_.Get());
    quality_scaler_resource_->RegisterAdaptationTaskQueue(
        resource_adaptation_queue_.Get());
    rtc::Event event;
    encoder_queue_.PostTask([this, &event] {
      quality_scaler_resource_->StartCheckForOveruse(
          VideoEncoder::QpThresholds());
      event.Set();
    });
    event.Wait(kDefaultTimeout);
  }

  ~QualityScalerResourceTest() {
    rtc::Event event;
    encoder_queue_.PostTask([this, &event] {
      quality_scaler_resource_->StopCheckForOveruse();
      event.Set();
    });
    event.Wait(kDefaultTimeout);
  }

 protected:
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::TaskQueue resource_adaptation_queue_;
  rtc::TaskQueue encoder_queue_;
  FakeDegradationPreferenceProvider degradation_preference_provider_;
  rtc::scoped_refptr<QualityScalerResource> quality_scaler_resource_;
};

TEST_F(QualityScalerResourceTest, ReportQpHigh) {
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  encoder_queue_.PostTask([this, callback] {
    quality_scaler_resource_->OnReportQpUsageHigh(callback);
  });
  callback->qp_usage_handled_event()->Wait(kDefaultTimeout);
}

TEST_F(QualityScalerResourceTest, ReportQpLow) {
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  encoder_queue_.PostTask([this, callback] {
    quality_scaler_resource_->OnReportQpUsageLow(callback);
  });
  callback->qp_usage_handled_event()->Wait(kDefaultTimeout);
}

TEST_F(QualityScalerResourceTest, MultipleCallbacksInFlight) {
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback1 =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback2 =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback3 =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  encoder_queue_.PostTask([this, callback1, callback2, callback3] {
    quality_scaler_resource_->OnReportQpUsageHigh(callback1);
    quality_scaler_resource_->OnReportQpUsageLow(callback2);
    quality_scaler_resource_->OnReportQpUsageHigh(callback3);
  });
  callback1->qp_usage_handled_event()->Wait(kDefaultTimeout);
  callback2->qp_usage_handled_event()->Wait(kDefaultTimeout);
  callback3->qp_usage_handled_event()->Wait(kDefaultTimeout);
}

TEST_F(QualityScalerResourceTest, AbortPendingCallbacksAndStartAgain) {
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback1 =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  rtc::scoped_refptr<FakeQualityScalerQpUsageHandlerCallback> callback2 =
      new FakeQualityScalerQpUsageHandlerCallback(&encoder_queue_);
  encoder_queue_.PostTask([this, callback1, callback2] {
    quality_scaler_resource_->OnReportQpUsageHigh(callback1);
    quality_scaler_resource_->StopCheckForOveruse();
    EXPECT_TRUE(callback1->qp_usage_handled_event()->Wait(0));
    quality_scaler_resource_->StartCheckForOveruse(
        VideoEncoder::QpThresholds());
    quality_scaler_resource_->OnReportQpUsageHigh(callback2);
  });
  callback1->qp_usage_handled_event()->Wait(kDefaultTimeout);
  callback2->qp_usage_handled_event()->Wait(kDefaultTimeout);
}

}  // namespace webrtc
