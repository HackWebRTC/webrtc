/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector_resource_adaptation_module.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "video/adaptation/adaptation_counters.h"

namespace webrtc {

TEST(OveruseFrameDetectorResourceAdaptationModuleTest,
     FirstAdaptationDown_Fps) {
  AdaptationCounters cpu;
  AdaptationCounters qp;
  AdaptationCounters total(0, 1);

  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);
  AdaptationCounters expected_cpu(0, 1);
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(OveruseFrameDetectorResourceAdaptationModuleTest,
     FirstAdaptationDown_Resolution) {
  AdaptationCounters cpu;
  AdaptationCounters qp;
  AdaptationCounters total(1, 0);

  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);
  AdaptationCounters expected_cpu(1, 0);
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(OveruseFrameDetectorResourceAdaptationModuleTest, LastAdaptUp_Fps) {
  AdaptationCounters cpu(0, 1);
  AdaptationCounters qp;
  AdaptationCounters total;

  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);
  AdaptationCounters expected_cpu;
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(OveruseFrameDetectorResourceAdaptationModuleTest, LastAdaptUp_Resolution) {
  AdaptationCounters cpu(1, 0);
  AdaptationCounters qp;
  AdaptationCounters total;

  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);
  AdaptationCounters expected_cpu;
  AdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(OveruseFrameDetectorResourceAdaptationModuleTest,
     AdaptUpWithBorrow_Resolution) {
  AdaptationCounters cpu(0, 1);
  AdaptationCounters qp(1, 0);
  AdaptationCounters total(0, 1);

  // CPU adaptation for resolution, but no resolution adaptation left from CPU.
  // We then borrow the resolution adaptation from qp, and give qp the fps
  // adaptation from CPU.
  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);

  AdaptationCounters expected_cpu(0, 0);
  AdaptationCounters expected_qp(0, 1);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(OveruseFrameDetectorResourceAdaptationModuleTest, AdaptUpWithBorrow_Fps) {
  AdaptationCounters cpu(1, 0);
  AdaptationCounters qp(0, 1);
  AdaptationCounters total(1, 0);

  // CPU adaptation for fps, but no fps adaptation left from CPU. We then borrow
  // the fps adaptation from qp, and give qp the resolution adaptation from CPU.
  OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
      total, &cpu, &qp);

  AdaptationCounters expected_cpu(0, 0);
  AdaptationCounters expected_qp(1, 0);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

}  // namespace webrtc
