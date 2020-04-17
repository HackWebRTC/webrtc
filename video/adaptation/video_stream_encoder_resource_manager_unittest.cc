/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_encoder_resource_manager.h"

#include "api/video/video_adaptation_counters.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(VideoStreamEncoderResourceManagerTest, FirstAdaptationDown_Fps) {
  VideoAdaptationCounters cpu;
  VideoAdaptationCounters qp;
  VideoAdaptationCounters total(0, 1);

  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);
  VideoAdaptationCounters expected_cpu(0, 1);
  VideoAdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(VideoStreamEncoderResourceManagerTest, FirstAdaptationDown_Resolution) {
  VideoAdaptationCounters cpu;
  VideoAdaptationCounters qp;
  VideoAdaptationCounters total(1, 0);

  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);
  VideoAdaptationCounters expected_cpu(1, 0);
  VideoAdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(VideoStreamEncoderResourceManagerTest, LastAdaptUp_Fps) {
  VideoAdaptationCounters cpu(0, 1);
  VideoAdaptationCounters qp;
  VideoAdaptationCounters total;

  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);
  VideoAdaptationCounters expected_cpu;
  VideoAdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(VideoStreamEncoderResourceManagerTest, LastAdaptUp_Resolution) {
  VideoAdaptationCounters cpu(1, 0);
  VideoAdaptationCounters qp;
  VideoAdaptationCounters total;

  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);
  VideoAdaptationCounters expected_cpu;
  VideoAdaptationCounters expected_qp;
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(VideoStreamEncoderResourceManagerTest, AdaptUpWithBorrow_Resolution) {
  VideoAdaptationCounters cpu(0, 1);
  VideoAdaptationCounters qp(1, 0);
  VideoAdaptationCounters total(0, 1);

  // CPU adaptation for resolution, but no resolution adaptation left from CPU.
  // We then borrow the resolution adaptation from qp, and give qp the fps
  // adaptation from CPU.
  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);

  VideoAdaptationCounters expected_cpu(0, 0);
  VideoAdaptationCounters expected_qp(0, 1);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

TEST(VideoStreamEncoderResourceManagerTest, AdaptUpWithBorrow_Fps) {
  VideoAdaptationCounters cpu(1, 0);
  VideoAdaptationCounters qp(0, 1);
  VideoAdaptationCounters total(1, 0);

  // CPU adaptation for fps, but no fps adaptation left from CPU. We then borrow
  // the fps adaptation from qp, and give qp the resolution adaptation from CPU.
  VideoStreamEncoderResourceManager::OnAdaptationCountChanged(total, &cpu, &qp);

  VideoAdaptationCounters expected_cpu(0, 0);
  VideoAdaptationCounters expected_qp(1, 0);
  EXPECT_EQ(expected_cpu, cpu);
  EXPECT_EQ(expected_qp, qp);
}

}  // namespace webrtc
