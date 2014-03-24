/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/overuse_frame_detector.h"

namespace webrtc {
namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameInterval33ms = 33;
  const int kProcessIntervalMs = 5000;
}  // namespace

class MockCpuOveruseObserver : public CpuOveruseObserver {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
  MOCK_METHOD0(NormalUsage, void());
};

class CpuOveruseObserverImpl : public CpuOveruseObserver {
 public:
  CpuOveruseObserverImpl() :
    overuse_(0),
    normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  void OveruseDetected() { ++overuse_; }
  void NormalUsage() { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get()));

    options_.low_capture_jitter_threshold_ms = 10.0f;
    options_.high_capture_jitter_threshold_ms = 15.0f;
    options_.min_process_count = 0;
    overuse_detector_->SetOptions(options_);
    overuse_detector_->SetObserver(observer_.get());
  }

  int InitialJitter() {
    return ((options_.low_capture_jitter_threshold_ms +
             options_.high_capture_jitter_threshold_ms) / 2.0f) + 0.5;
  }

  int InitialEncodeUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) / 2.0f) + 0.5;
  }

  void InsertFramesWithInterval(
      size_t num_frames, int interval_ms, int width, int height) {
    while (num_frames-- > 0) {
      clock_->AdvanceTimeMilliseconds(interval_ms);
      overuse_detector_->FrameCaptured(width, height);
    }
  }

  void InsertAndEncodeFramesWithInterval(
      int num_frames, int interval_ms, int width, int height, int encode_ms) {
    while (num_frames-- > 0) {
      overuse_detector_->FrameCaptured(width, height);
      clock_->AdvanceTimeMilliseconds(encode_ms);
      overuse_detector_->FrameEncoded(encode_ms);
      clock_->AdvanceTimeMilliseconds(interval_ms - encode_ms);
    }
  }

  void TriggerOveruse(int num_times) {
    for (int i = 0; i < num_times; ++i) {
      InsertFramesWithInterval(200, kFrameInterval33ms, kWidth, kHeight);
      InsertFramesWithInterval(50, 110, kWidth, kHeight);
      overuse_detector_->Process();
    }
  }

  void TriggerNormalUsage() {
    InsertFramesWithInterval(900, kFrameInterval33ms, kWidth, kHeight);
    overuse_detector_->Process();
  }

  void TriggerOveruseWithEncodeUsage(int num_times) {
    const int kEncodeTimeMs = 32;
    for (int i = 0; i < num_times; ++i) {
      InsertAndEncodeFramesWithInterval(
          1000, kFrameInterval33ms, kWidth, kHeight, kEncodeTimeMs);
      overuse_detector_->Process();
    }
  }

  void TriggerNormalUsageWithEncodeUsage() {
    const int kEncodeTimeMs = 5;
    InsertAndEncodeFramesWithInterval(
        1000, kFrameInterval33ms, kWidth, kHeight, kEncodeTimeMs);
    overuse_detector_->Process();
  }

  CpuOveruseOptions options_;
  scoped_ptr<SimulatedClock> clock_;
  scoped_ptr<MockCpuOveruseObserver> observer_;
  scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverNoObserver) {
  overuse_detector_->SetObserver(NULL);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverDisabled) {
  options_.enable_capture_jitter_method = false;
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, TriggerNormalUsageWithMinProcessCount) {
  CpuOveruseObserverImpl overuse_observer_;
  overuse_detector_->SetObserver(&overuse_observer_);
  options_.min_process_count = 1;
  overuse_detector_->SetOptions(options_);
  InsertFramesWithInterval(900, kFrameInterval33ms, kWidth, kHeight);
  overuse_detector_->Process();
  EXPECT_EQ(0, overuse_observer_.normaluse_);
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  overuse_detector_->Process();
  EXPECT_EQ(1, overuse_observer_.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(64);
  for(size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, CaptureJitter) {
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), overuse_detector_->CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterResolutionChange) {
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), overuse_detector_->CaptureJitterMs());
  // Verify reset.
  InsertFramesWithInterval(1, kFrameInterval33ms, kWidth, kHeight + 1);
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterFrameTimeout) {
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), overuse_detector_->CaptureJitterMs());
  InsertFramesWithInterval(
      1, options_.frame_timeout_interval_ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), overuse_detector_->CaptureJitterMs());
  // Verify reset.
  InsertFramesWithInterval(
      1, options_.frame_timeout_interval_ms + 1, kWidth, kHeight);
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterChangingThreshold) {
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
  options_.high_capture_jitter_threshold_ms = 90.0f;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
  options_.low_capture_jitter_threshold_ms = 30.0f;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdatingCaptureJitter) {
  options_.min_frame_samples = 40;
  overuse_detector_->SetOptions(options_);
  InsertFramesWithInterval(40, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_EQ(InitialJitter(), overuse_detector_->CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, NoCaptureQueueDelay) {
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelay) {
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayMultipleFrames) {
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 30);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayResetAtResolutionSwitch) {
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(kWidth, kHeight + 1);
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayNoMatchingCapturedFrame) {
  overuse_detector_->FrameCaptured(kWidth, kHeight);
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
  // No new captured frame. The last delay should be reported.
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, EncodedFrame) {
  const int kInitialAvgEncodeTimeInMs = 5;
  EXPECT_EQ(kInitialAvgEncodeTimeInMs, overuse_detector_->AvgEncodeTimeMs());
  for (int i = 0; i < 30; i++) {
    clock_->AdvanceTimeMilliseconds(33);
    overuse_detector_->FrameEncoded(2);
  }
  EXPECT_EQ(2, overuse_detector_->AvgEncodeTimeMs());
}

TEST_F(OveruseFrameDetectorTest, InitialEncodeUsage) {
  EXPECT_EQ(InitialEncodeUsage(), overuse_detector_->EncodeUsagePercent());
}

TEST_F(OveruseFrameDetectorTest, EncodedUsage) {
  const int kEncodeTimeMs = 5;
  InsertAndEncodeFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kEncodeTimeMs);
  EXPECT_EQ(15, overuse_detector_->EncodeUsagePercent());
}

TEST_F(OveruseFrameDetectorTest, EncodeUsageResetAfterChangingThreshold) {
  EXPECT_EQ(InitialEncodeUsage(), overuse_detector_->EncodeUsagePercent());
  options_.high_encode_usage_threshold_percent = 100;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialEncodeUsage(), overuse_detector_->EncodeUsagePercent());
  options_.low_encode_usage_threshold_percent = 20;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialEncodeUsage(), overuse_detector_->EncodeUsagePercent());
}

TEST_F(OveruseFrameDetectorTest, TriggerOveruseWithEncodeUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithEncodeUsage(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithEncodeUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithEncodeUsage(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerNormalUsageWithEncodeUsage();
}
}  // namespace webrtc
