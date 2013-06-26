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
#include "webrtc/video_engine/overuse_frame_detector.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

const int kProcessIntervalMs = 2000;

class MockOveruseObserver : public OveruseObserver {
 public:
  MockOveruseObserver() {}
  virtual ~MockOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
};

class OveruseFrameDetectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockOveruseObserver());
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get(),
                                                     observer_.get()));
  }
  scoped_ptr<SimulatedClock> clock_;
  scoped_ptr<MockOveruseObserver> observer_;
  scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  EXPECT_EQ(overuse_detector_->TimeUntilNextProcess(), kProcessIntervalMs);
  overuse_detector_->CapturedFrame();
  overuse_detector_->EncodedFrame();
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();

  overuse_detector_->CapturedFrame();
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  overuse_detector_->Process();

  clock_->AdvanceTimeMilliseconds(5000);
  overuse_detector_->CapturedFrame();
  overuse_detector_->EncodedFrame();
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  overuse_detector_->Process();
}
}  // namespace webrtc
