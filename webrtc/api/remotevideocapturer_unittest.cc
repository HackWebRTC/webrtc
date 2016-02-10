/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/api/remotevideocapturer.h"
#include "webrtc/base/gunit.h"
#include "webrtc/media/webrtc/webrtcvideoframe.h"

using cricket::CaptureState;
using cricket::VideoCapturer;
using cricket::VideoFormat;
using cricket::VideoFormatPod;
using cricket::VideoFrame;

static const int kMaxWaitMs = 1000;
static const VideoFormatPod kTestFormat =
    {640, 480, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY};

class RemoteVideoCapturerTest : public testing::Test,
                                public sigslot::has_slots<> {
 protected:
  RemoteVideoCapturerTest()
      : captured_frame_num_(0),
        capture_state_(cricket::CS_STOPPED) {}

  virtual void SetUp() {
    capturer_.SignalStateChange.connect(
        this, &RemoteVideoCapturerTest::OnStateChange);
    capturer_.SignalVideoFrame.connect(
        this, &RemoteVideoCapturerTest::OnVideoFrame);
  }

  ~RemoteVideoCapturerTest() {
    capturer_.SignalStateChange.disconnect(this);
    capturer_.SignalVideoFrame.disconnect(this);
  }

  int captured_frame_num() const {
    return captured_frame_num_;
  }

  CaptureState capture_state() const {
    return capture_state_;
  }

  webrtc::RemoteVideoCapturer capturer_;

 private:
  void OnStateChange(VideoCapturer* capturer,
                     CaptureState capture_state) {
    EXPECT_EQ(&capturer_, capturer);
    capture_state_ = capture_state;
  }

  void OnVideoFrame(VideoCapturer* capturer, const VideoFrame* frame) {
    EXPECT_EQ(&capturer_, capturer);
    ++captured_frame_num_;
  }

  int captured_frame_num_;
  CaptureState capture_state_;
};

TEST_F(RemoteVideoCapturerTest, StartStop) {
  // Start
  EXPECT_TRUE(
      capturer_.StartCapturing(VideoFormat(kTestFormat)));
  EXPECT_TRUE_WAIT((cricket::CS_RUNNING == capture_state()), kMaxWaitMs);
  EXPECT_EQ(VideoFormat(kTestFormat),
            *capturer_.GetCaptureFormat());
  EXPECT_TRUE(capturer_.IsRunning());

  // Stop
  capturer_.Stop();
  EXPECT_TRUE_WAIT((cricket::CS_STOPPED == capture_state()), kMaxWaitMs);
  EXPECT_TRUE(NULL == capturer_.GetCaptureFormat());
}

TEST_F(RemoteVideoCapturerTest, GetPreferredFourccs) {
  EXPECT_FALSE(capturer_.GetPreferredFourccs(NULL));

  std::vector<uint32_t> fourccs;
  EXPECT_TRUE(capturer_.GetPreferredFourccs(&fourccs));
  EXPECT_EQ(1u, fourccs.size());
  EXPECT_EQ(cricket::FOURCC_I420, fourccs.at(0));
}

TEST_F(RemoteVideoCapturerTest, GetBestCaptureFormat) {
  VideoFormat desired = VideoFormat(kTestFormat);
  EXPECT_FALSE(capturer_.GetBestCaptureFormat(desired, NULL));

  VideoFormat expected_format = VideoFormat(kTestFormat);
  expected_format.fourcc = cricket::FOURCC_I420;
  VideoFormat best_format;
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best_format));
  EXPECT_EQ(expected_format, best_format);
}

TEST_F(RemoteVideoCapturerTest, InputFrame) {
  EXPECT_EQ(0, captured_frame_num());

  cricket::WebRtcVideoFrame test_frame;
  capturer_.SignalVideoFrame(&capturer_, &test_frame);
  EXPECT_EQ(1, captured_frame_num());
  capturer_.SignalVideoFrame(&capturer_, &test_frame);
  EXPECT_EQ(2, captured_frame_num());
}
