/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>

#include "talk/app/webrtc/remotevideocapturer.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "webrtc/base/gunit.h"

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
