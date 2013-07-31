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

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

const int kProcessIntervalMs = 2000;
const int kOveruseHistoryMs = 5000;
const int kMinCallbackDeltaMs = 30000;
const int64_t kMinValidHistoryMs = kOveruseHistoryMs / 2;

class MockCpuOveruseObserver : public CpuOveruseObserver {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
  MOCK_METHOD0(NormalUsage, void());
};

class OveruseFrameDetectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get()));
    overuse_detector_->SetObserver(observer_.get());
  }

  void CaptureAndEncodeFrames(int num_frames, int64_t frame_interval_ms,
                              int encode_time_ms, size_t width, size_t height) {
    for (int frame = 0; frame < num_frames; ++frame) {
      overuse_detector_->FrameCaptured();
      overuse_detector_->FrameEncoded(encode_time_ms, width, height);
      clock_->AdvanceTimeMilliseconds(frame_interval_ms);
    }
  }

  void CaptureAndEncodeWithOveruse(int overuse_time_ms,
                                   int64_t frame_interval_ms,
                                   int64_t encode_time_ms, size_t width,
                                   size_t height) {
    // 'encodes_before_dropping' is derived from 'kMinEncodeRatio' in
    // 'overuse_frame_detector.h'.
    const int encodes_before_dropping = 14;
    for (int time_ms = 0; time_ms < overuse_time_ms;
         time_ms += frame_interval_ms * (1 + encodes_before_dropping)) {
      CaptureAndEncodeFrames(encodes_before_dropping, frame_interval_ms,
                             encode_time_ms, width, height);
      overuse_detector_->FrameCaptured();
      clock_->AdvanceTimeMilliseconds(frame_interval_ms);
    }
  }

  scoped_ptr<SimulatedClock> clock_;
  scoped_ptr<MockCpuOveruseObserver> observer_;
  scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  EXPECT_EQ(overuse_detector_->TimeUntilNextProcess(), kProcessIntervalMs);

  // Enough history to trigger an overuse, but life is good so far.
  int frame_interval_ms = 33;
  int num_frames = kMinValidHistoryMs / frame_interval_ms + 1;
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 2, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  // Trigger an overuse.
  CaptureAndEncodeWithOveruse(kOveruseHistoryMs, frame_interval_ms, 2, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  overuse_detector_->set_underuse_encode_timing_enabled(true);
  // Start with triggering an overuse.
  // A new resolution will trigger a reset, so add one frame to get going.
  int frame_interval_ms = 33;
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs, frame_interval_ms, 2, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  // Make everything good again, but don't advance time long enough to trigger
  // an underuse.
  int num_frames = kOveruseHistoryMs / frame_interval_ms;
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 1, 1, 1);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  // Advance time long enough to trigger an increase callback.
  num_frames = (kMinCallbackDeltaMs - kOveruseHistoryMs + 1) /
      (frame_interval_ms - 0.5f);
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 1, 1, 1);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(1);
  overuse_detector_->Process();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  overuse_detector_->set_underuse_encode_timing_enabled(true);
  // Start with triggering an overuse.
  int frame_interval_ms = 33;
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs, frame_interval_ms, 16, 4, 4);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  CaptureAndEncodeWithOveruse(kOveruseHistoryMs, frame_interval_ms, 4, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  // Let life be good again and wait for an underuse callback.
  int num_frames = kMinCallbackDeltaMs / (frame_interval_ms - 0.5f);
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 1, 1, 1);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(1);
  overuse_detector_->Process();

  // And one more.
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 4, 2, 2);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(1);
  overuse_detector_->Process();

  // But no more since we're at the max resolution.
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 4, 4, 4);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  overuse_detector_->Process();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndNoRecovery) {
  overuse_detector_->set_underuse_encode_timing_enabled(true);
  // Start with triggering an overuse.
  int frame_interval_ms = 33;
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs, frame_interval_ms, 4, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  // Everything is fine, but we haven't waited long enough to trigger an
  // increase callback.
  CaptureAndEncodeFrames(30, 33, 3, 1, 1);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  // Advance time enough to trigger an increase callback, but encode time
  // shouldn't have decreased enough to try an increase.
  int num_frames = kMinCallbackDeltaMs / (frame_interval_ms - 0.5f);
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 3, 1, 1);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  overuse_detector_->Process();
}

TEST_F(OveruseFrameDetectorTest, NoEncodeTimeForUnderuse) {
  overuse_detector_->set_underuse_encode_timing_enabled(false);
  // Start with triggering an overuse.
  int frame_interval_ms = 33;
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs, frame_interval_ms, 4, 2, 2);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  // Everything is fine, but we haven't waited long enough to trigger an
  // increase callback.
  int num_frames = 1000 / (frame_interval_ms - 0.5f);
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 3, 1, 1);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  // Advance time enough to allow underuse, but keep encode time too high to
  // trigger an underuse if accounted for, see 'OveruseAndNoRecovery' test case.
  num_frames = kMinCallbackDeltaMs / (frame_interval_ms - 0.5f);
  CaptureAndEncodeFrames(num_frames, frame_interval_ms, 3, 1, 1);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(1);
  overuse_detector_->Process();
}

TEST_F(OveruseFrameDetectorTest, ResolutionChange) {
  overuse_detector_->set_underuse_encode_timing_enabled(true);
  int frame_interval_ms = 33;
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs / 2, frame_interval_ms, 3, 1,
                              1);

  // Keep overusing, but with a new resolution.
  CaptureAndEncodeWithOveruse(kMinValidHistoryMs - frame_interval_ms,
                              frame_interval_ms, 4, 2, 2);

  // Enough samples and time to trigger an overuse, but resolution reset should
  // prevent this.
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  // Fill the history.
  CaptureAndEncodeFrames(2, kOveruseHistoryMs / 2, 3, 1, 1);

  // Capture a frame without finish encoding to trigger an overuse.
  overuse_detector_->FrameCaptured();
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();
}
}  // namespace webrtc
