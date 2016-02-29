/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/media/base/videobroadcaster.h"
#include "webrtc/media/engine/webrtcvideoframe.h"

using rtc::VideoBroadcaster;
using rtc::VideoSinkWants;
using cricket::WebRtcVideoFrame;

namespace {

class TestSink : public rtc::VideoSinkInterface<cricket::VideoFrame> {
 public:
  void OnFrame(const cricket::VideoFrame& frame) override {
    ++number_of_rendered_frames_;
  }

  int number_of_rendered_frames_ = 0;
};

}  // namespace

TEST(VideoBroadcasterTest, frame_wanted) {
  VideoBroadcaster broadcaster;
  EXPECT_FALSE(broadcaster.frame_wanted());

  TestSink sink;
  broadcaster.AddOrUpdateSink(&sink, rtc::VideoSinkWants());
  EXPECT_TRUE(broadcaster.frame_wanted());

  broadcaster.RemoveSink(&sink);
  EXPECT_FALSE(broadcaster.frame_wanted());
}

TEST(VideoBroadcasterTest, OnFrame) {
  VideoBroadcaster broadcaster;

  TestSink sink1;
  TestSink sink2;
  broadcaster.AddOrUpdateSink(&sink1, rtc::VideoSinkWants());
  broadcaster.AddOrUpdateSink(&sink2, rtc::VideoSinkWants());

  WebRtcVideoFrame frame;

  broadcaster.OnFrame(frame);
  EXPECT_EQ(1, sink1.number_of_rendered_frames_);
  EXPECT_EQ(1, sink2.number_of_rendered_frames_);

  broadcaster.RemoveSink(&sink1);
  broadcaster.OnFrame(frame);
  EXPECT_EQ(1, sink1.number_of_rendered_frames_);
  EXPECT_EQ(2, sink2.number_of_rendered_frames_);

  broadcaster.AddOrUpdateSink(&sink1, rtc::VideoSinkWants());
  broadcaster.OnFrame(frame);
  EXPECT_EQ(2, sink1.number_of_rendered_frames_);
  EXPECT_EQ(3, sink2.number_of_rendered_frames_);
}

TEST(VideoBroadcasterTest, AppliesRotationIfAnySinkWantsRotationApplied) {
  VideoBroadcaster broadcaster;
  EXPECT_TRUE(broadcaster.wants().rotation_applied);

  TestSink sink1;
  VideoSinkWants wants1;
  wants1.rotation_applied = false;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_FALSE(broadcaster.wants().rotation_applied);

  TestSink sink2;
  VideoSinkWants wants2;
  wants2.rotation_applied = true;

  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_TRUE(broadcaster.wants().rotation_applied);

  broadcaster.RemoveSink(&sink2);
  EXPECT_FALSE(broadcaster.wants().rotation_applied);
}

TEST(VideoBroadcasterTest, AppliesMinOfSinkWantsMaxPixelCount) {
  VideoBroadcaster broadcaster;
  EXPECT_TRUE(!broadcaster.wants().max_pixel_count);

  TestSink sink1;
  VideoSinkWants wants1;
  wants1.max_pixel_count = rtc::Optional<int>(1280 * 720);

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().max_pixel_count);

  TestSink sink2;
  VideoSinkWants wants2;
  wants2.max_pixel_count = rtc::Optional<int>(640 * 360);
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(640 * 360, *broadcaster.wants().max_pixel_count);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().max_pixel_count);
}

TEST(VideoBroadcasterTest, AppliesMinOfSinkWantsMaxPixelCountStepUp) {
  VideoBroadcaster broadcaster;
  EXPECT_TRUE(!broadcaster.wants().max_pixel_count_step_up);

  TestSink sink1;
  VideoSinkWants wants1;
  wants1.max_pixel_count_step_up = rtc::Optional<int>(1280 * 720);

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().max_pixel_count_step_up);

  TestSink sink2;
  VideoSinkWants wants2;
  wants2.max_pixel_count_step_up = rtc::Optional<int>(640 * 360);
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(640 * 360, *broadcaster.wants().max_pixel_count_step_up);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().max_pixel_count_step_up);
}
