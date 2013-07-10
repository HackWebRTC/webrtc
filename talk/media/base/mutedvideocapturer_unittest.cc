/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/media/base/mutedvideocapturer.h"

#include "talk/base/gunit.h"
#include "talk/media/base/videoframe.h"

class MutedVideoCapturerTest : public sigslot::has_slots<>,
                               public testing::Test {
 protected:
  void SetUp() {
    frames_received_ = 0;
    capturer_.SignalVideoFrame
        .connect(this, &MutedVideoCapturerTest::OnVideoFrame);
  }
  void OnVideoFrame(cricket::VideoCapturer* capturer,
                    const cricket::VideoFrame* muted_frame) {
    EXPECT_EQ(capturer, &capturer_);
    ++frames_received_;
    received_width_ = muted_frame->GetWidth();
    received_height_ = muted_frame->GetHeight();
  }
  int frames_received() { return frames_received_; }
  bool ReceivedCorrectFormat() {
    return (received_width_ == capturer_.GetCaptureFormat()->width) &&
           (received_height_ == capturer_.GetCaptureFormat()->height);
  }

  cricket::MutedVideoCapturer capturer_;
  int frames_received_;
  cricket::VideoFormat capture_format_;
  int received_width_;
  int received_height_;
};

TEST_F(MutedVideoCapturerTest, GetBestCaptureFormat) {
  cricket::VideoFormat format(640, 360, cricket::VideoFormat::FpsToInterval(30),
                              cricket::FOURCC_I420);
  cricket::VideoFormat best_format;
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(format, &best_format));
  EXPECT_EQ(format.width, best_format.width);
  EXPECT_EQ(format.height, best_format.height);
  EXPECT_EQ(format.interval, best_format.interval);
  EXPECT_EQ(format.fourcc, best_format.fourcc);
}

TEST_F(MutedVideoCapturerTest, IsScreencast) {
  EXPECT_FALSE(capturer_.IsScreencast());
}

TEST_F(MutedVideoCapturerTest, GetPreferredFourccs) {
  std::vector<uint32> fourccs;
  EXPECT_TRUE(capturer_.GetPreferredFourccs(&fourccs));
  EXPECT_EQ(fourccs.size(), 1u);
  EXPECT_TRUE(capturer_.GetPreferredFourccs(&fourccs));
  EXPECT_EQ(fourccs.size(), 1u);
  EXPECT_EQ(fourccs[0], cricket::FOURCC_I420);
}

TEST_F(MutedVideoCapturerTest, Capturing) {
  cricket::VideoFormat format(640, 360, cricket::VideoFormat::FpsToInterval(30),
                              cricket::FOURCC_I420);
  EXPECT_EQ(capturer_.Start(format), cricket::CS_RUNNING);
  EXPECT_EQ(capturer_.Start(format), cricket::CS_RUNNING);
  EXPECT_TRUE(capturer_.IsRunning());
  // 100 ms should be enough to receive 3 frames at FPS of 30.
  EXPECT_EQ_WAIT(frames_received(), 1, 100);
  EXPECT_TRUE(ReceivedCorrectFormat());
  capturer_.Stop();
  EXPECT_FALSE(capturer_.IsRunning());
}
