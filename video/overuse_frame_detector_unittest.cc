/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/event.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

using ::testing::InvokeWithoutArgs;
using ::testing::AtLeast;
using ::testing::_;

namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  const int kProcessTimeUs = 5 * rtc::kNumMicrosecsPerMillisec;
}  // namespace

class MockCpuOveruseObserver : public AdaptationObserverInterface {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD1(AdaptUp, void(AdaptReason));
  MOCK_METHOD1(AdaptDown, void(AdaptReason));
};

class CpuOveruseObserverImpl : public AdaptationObserverInterface {
 public:
  CpuOveruseObserverImpl() :
    overuse_(0),
    normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  void AdaptDown(AdaptReason) { ++overuse_; }
  void AdaptUp(AdaptReason) { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorUnderTest : public OveruseFrameDetector {
 public:
  OveruseFrameDetectorUnderTest(const CpuOveruseOptions& options,
                                AdaptationObserverInterface* overuse_observer,
                                EncodedFrameObserver* encoder_timing,
                                CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(options,
                             overuse_observer,
                             encoder_timing,
                             metrics_observer) {}
  ~OveruseFrameDetectorUnderTest() {}

  using OveruseFrameDetector::CheckForOveruse;
};

class OveruseFrameDetectorTest : public ::testing::Test,
                                 public CpuOveruseMetricsObserver {
 protected:
  void SetUp() override {
    observer_.reset(new MockCpuOveruseObserver());
    options_.min_process_count = 0;
    ReinitializeOveruseDetector();
  }

  void ReinitializeOveruseDetector() {
    overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
        options_, observer_.get(), nullptr, this));
  }

  void OnEncodedFrameTimeMeasured(int encode_time_ms,
                                  const CpuOveruseMetrics& metrics) override {
    metrics_ = metrics;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) / 2.0f) + 0.5;
  }

  void InsertAndSendFramesWithInterval(int num_frames,
                                       int interval_us,
                                       int width,
                                       int height,
                                       int delay_us) {
    while (num_frames-- > 0) {
      overuse_detector_->FrameCaptured(width, height);
      overuse_detector_->FrameEncoded(rtc::TimeMicros(), delay_us);
      clock_.AdvanceTimeMicros(interval_us);
    }
  }

  void ForceUpdate(int width, int height) {
    // This is mainly to check initial values and whether the overuse
    // detector has been reset or not.
    InsertAndSendFramesWithInterval(1, rtc::kNumMicrosecsPerSec, width, height,
                                    kFrameIntervalUs);
  }

  void TriggerOveruse(int num_times) {
    const int kDelayUs = 32 * rtc::kNumMicrosecsPerMillisec;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(
          1000, kFrameIntervalUs, kWidth, kHeight, kDelayUs);
      overuse_detector_->CheckForOveruse();
    }
  }

  void TriggerUnderuse() {
    const int kDelayUs1 = 5000;
    const int kDelayUs2 = 6000;
    InsertAndSendFramesWithInterval(
        1300, kFrameIntervalUs, kWidth, kHeight, kDelayUs1);
    InsertAndSendFramesWithInterval(
        1, kFrameIntervalUs, kWidth, kHeight, kDelayUs2);
    overuse_detector_->CheckForOveruse();
  }

  int UsagePercent() { return metrics_.encode_usage_percent; }

  int64_t OveruseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.high_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  int64_t UnderuseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.low_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  CpuOveruseOptions options_;
  rtc::ScopedFakeClock clock_;
  std::unique_ptr<MockCpuOveruseObserver> observer_;
  std::unique_ptr<OveruseFrameDetectorUnderTest> overuse_detector_;
  CpuOveruseMetrics metrics_;

  static const auto reason_ = AdaptationObserverInterface::AdaptReason::kCpu;
};


// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithNoObserver) {
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      options_, nullptr, nullptr, this));
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(0);
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  const int kProcessIntervalUs = 5 * rtc::kNumMicrosecsPerSec;
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      options_, &overuse_observer, nullptr, this));
  InsertAndSendFramesWithInterval(
      1200, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_.AdvanceTimeMicros(kProcessIntervalUs);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(0);
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_EQ(kProcessTimeUs * 100 / kFrameIntervalUs, UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms *
      rtc::kNumMicrosecsPerMillisec, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, (options_.frame_timeout_interval_ms + 1) *
      rtc::kNumMicrosecsPerMillisec, kWidth, kHeight, kProcessTimeUs);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, RunOnTqNormalUsage) {
  rtc::TaskQueue queue("OveruseFrameDetectorTestQueue");

  rtc::Event event(false, false);
  queue.PostTask([this, &event] {
    overuse_detector_->StartCheckForOveruse();
    event.Set();
  });
  event.Wait(rtc::Event::kForever);

  // Expect NormalUsage(). When called, stop the |overuse_detector_| and then
  // set |event| to end the test.
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_))
      .WillOnce(InvokeWithoutArgs([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this] {
    const int kDelayUs1 = 5 * rtc::kNumMicrosecsPerMillisec;
    const int kDelayUs2 = 6 * rtc::kNumMicrosecsPerMillisec;
    InsertAndSendFramesWithInterval(1300, kFrameIntervalUs, kWidth, kHeight,
                                    kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kWidth, kHeight,
                                    kDelayUs2);
  });

  EXPECT_TRUE(event.Wait(10000));
}

// Models screencast, with irregular arrival of frames which are heavy
// to encode.
TEST_F(OveruseFrameDetectorTest, NoOveruseForLargeRandomFrameInterval) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(_)).Times(0);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(AtLeast(1));

  const int kNumFrames = 500;
  const int kEncodeTimeUs = 100 * rtc::kNumMicrosecsPerMillisec;

  const int kMinIntervalUs = 30 * rtc::kNumMicrosecsPerMillisec;
  const int kMaxIntervalUs = 1000 * rtc::kNumMicrosecsPerMillisec;

  webrtc::Random random(17);

  for (int i = 0; i < kNumFrames; i++) {
    int interval_us = random.Rand(kMinIntervalUs, kMaxIntervalUs);
    overuse_detector_->FrameCaptured(kWidth, kHeight);
    overuse_detector_->FrameEncoded(rtc::TimeMicros(), kEncodeTimeUs);

    overuse_detector_->CheckForOveruse();
    clock_.AdvanceTimeMicros(interval_us);
  }
  // Average usage 19%. Check that estimate is in the right ball park.
  EXPECT_NEAR(UsagePercent(), 20, 10);
}

// Models screencast, with irregular arrival of frames, often
// exceeding the timeout interval.
TEST_F(OveruseFrameDetectorTest, NoOveruseForRandomFrameIntervalWithReset) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(_)).Times(0);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(AtLeast(1));

  const int kNumFrames = 500;
  const int kEncodeTimeUs = 100 * rtc::kNumMicrosecsPerMillisec;

  const int kMinIntervalUs = 30 * rtc::kNumMicrosecsPerMillisec;
  const int kMaxIntervalUs = 3000 * rtc::kNumMicrosecsPerMillisec;

  webrtc::Random random(17);

  for (int i = 0; i < kNumFrames; i++) {
    int interval_us = random.Rand(kMinIntervalUs, kMaxIntervalUs);
    overuse_detector_->FrameCaptured(kWidth, kHeight);
    overuse_detector_->FrameEncoded(rtc::TimeMicros(), kEncodeTimeUs);

    overuse_detector_->CheckForOveruse();
    clock_.AdvanceTimeMicros(interval_us);
  }
  // Average usage 6.6%, but since the frame_timeout_interval_ms is
  // only 1500 ms, we often reset the estimate to the initial value.
  // Check that estimate is in the right ball park.
  EXPECT_GE(UsagePercent(), 1);
  EXPECT_LE(UsagePercent(), InitialUsage() + 5);
}

}  // namespace webrtc
