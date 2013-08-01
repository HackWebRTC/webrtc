/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/media/base/capturemanager.h"

#include "talk/base/gunit.h"
#include "talk/base/sigslot.h"
#include "talk/media/base/fakemediaprocessor.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/fakevideorenderer.h"

const int kMsCallbackWait = 50;

const int kFps = 30;
const cricket::VideoFormatPod kCameraFormats[] = {
  {640, 480, cricket::VideoFormat::FpsToInterval(kFps), cricket::FOURCC_I420},
  {320, 240, cricket::VideoFormat::FpsToInterval(kFps), cricket::FOURCC_I420}
};

class CaptureManagerTest : public ::testing::Test, public sigslot::has_slots<> {
 public:
  CaptureManagerTest()
      : capture_manager_(),
        callback_count_(0),
        format_vga_(kCameraFormats[0]),
        format_qvga_(kCameraFormats[1]) {
  }
  virtual void SetUp() {
    PopulateSupportedFormats();
    capture_state_ = cricket::CS_STOPPED;
    capture_manager_.SignalCapturerStateChange.connect(
        this,
        &CaptureManagerTest::OnCapturerStateChange);
  }
  void PopulateSupportedFormats() {
    std::vector<cricket::VideoFormat> formats;
    for (int i = 0; i < ARRAY_SIZE(kCameraFormats); ++i) {
      formats.push_back(cricket::VideoFormat(kCameraFormats[i]));
    }
    video_capturer_.ResetSupportedFormats(formats);
  }
  int NumFramesProcessed() { return media_processor_.video_frame_count(); }
  int NumFramesRendered() { return video_renderer_.num_rendered_frames(); }
  bool WasRenderedResolution(cricket::VideoFormat format) {
    return format.width == video_renderer_.width() &&
        format.height == video_renderer_.height();
  }
  cricket::CaptureState capture_state() { return capture_state_; }
  int callback_count() { return callback_count_; }
  void OnCapturerStateChange(cricket::VideoCapturer* capturer,
                             cricket::CaptureState capture_state) {
    capture_state_ = capture_state;
    ++callback_count_;
  }

 protected:
  cricket::FakeMediaProcessor media_processor_;
  cricket::FakeVideoCapturer video_capturer_;
  cricket::FakeVideoRenderer video_renderer_;

  cricket::CaptureManager capture_manager_;

  cricket::CaptureState capture_state_;
  int callback_count_;
  cricket::VideoFormat format_vga_;
  cricket::VideoFormat format_qvga_;
};

// Incorrect use cases.
TEST_F(CaptureManagerTest, InvalidCallOrder) {
  // Capturer must be registered before any of these calls.
  EXPECT_FALSE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                 &video_renderer_));
  EXPECT_FALSE(capture_manager_.AddVideoProcessor(&video_capturer_,
                                                  &media_processor_));
}

TEST_F(CaptureManagerTest, InvalidAddingRemoving) {
  EXPECT_FALSE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                 cricket::VideoFormat()));
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  EXPECT_FALSE(capture_manager_.AddVideoRenderer(&video_capturer_, NULL));
  EXPECT_FALSE(capture_manager_.RemoveVideoRenderer(&video_capturer_,
                                                    &video_renderer_));
  EXPECT_FALSE(capture_manager_.AddVideoProcessor(&video_capturer_,
                                                  NULL));
  EXPECT_FALSE(capture_manager_.RemoveVideoProcessor(&video_capturer_,
                                                     &media_processor_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_, format_vga_));
}

// Valid use cases
TEST_F(CaptureManagerTest, ProcessorTest) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  EXPECT_TRUE(capture_manager_.AddVideoProcessor(&video_capturer_,
                                                 &media_processor_));
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesProcessed());
  EXPECT_EQ(1, NumFramesRendered());
  EXPECT_TRUE(capture_manager_.RemoveVideoProcessor(&video_capturer_,
                                                    &media_processor_));
  // Processor has been removed so no more frames should be processed.
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesProcessed());
  EXPECT_EQ(2, NumFramesRendered());
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_, format_vga_));
  EXPECT_EQ(2, callback_count());
}

TEST_F(CaptureManagerTest, KeepFirstResolutionHigh) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesRendered());
  // Renderer should be fed frames with the resolution of format_vga_.
  EXPECT_TRUE(WasRenderedResolution(format_vga_));

  // Start again with one more format.
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_qvga_));
  // Existing renderers should be fed frames with the resolution of format_vga_.
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_TRUE(WasRenderedResolution(format_vga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_, format_vga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_qvga_));
  EXPECT_FALSE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_FALSE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                 format_qvga_));
}

// Should pick the lowest resolution as the highest resolution is not chosen
// until after capturing has started. This ensures that no particular resolution
// is favored over others.
TEST_F(CaptureManagerTest, KeepFirstResolutionLow) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_qvga_));
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  EXPECT_EQ_WAIT(1, callback_count(), kMsCallbackWait);
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesRendered());
  EXPECT_TRUE(WasRenderedResolution(format_qvga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_qvga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_vga_));
}

// Ensure that the reference counting is working when multiple start and
// multiple stop calls are made.
TEST_F(CaptureManagerTest, MultipleStartStops) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  // Add video capturer but with different format.
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_qvga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  // Ensure that a frame can be captured when two start calls have been made.
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesRendered());

  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_, format_vga_));
  // Video should still render since there has been two start calls but only
  // one stop call.
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(2, NumFramesRendered());

  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_qvga_));
  EXPECT_EQ_WAIT(cricket::CS_STOPPED, capture_state(), kMsCallbackWait);
  EXPECT_EQ(2, callback_count());
  // Last stop call should fail as it is one more than the number of start
  // calls.
  EXPECT_FALSE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                 format_vga_));
}

TEST_F(CaptureManagerTest, TestForceRestart) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_qvga_));
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  EXPECT_EQ_WAIT(1, callback_count(), kMsCallbackWait);
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesRendered());
  EXPECT_TRUE(WasRenderedResolution(format_qvga_));
  // Now restart with vga.
  EXPECT_TRUE(capture_manager_.RestartVideoCapture(
      &video_capturer_, format_qvga_, format_vga_,
      cricket::CaptureManager::kForceRestart));
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(2, NumFramesRendered());
  EXPECT_TRUE(WasRenderedResolution(format_vga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_vga_));
}

TEST_F(CaptureManagerTest, TestRequestRestart) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_TRUE(capture_manager_.AddVideoRenderer(&video_capturer_,
                                                &video_renderer_));
  EXPECT_EQ_WAIT(1, callback_count(), kMsCallbackWait);
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(1, NumFramesRendered());
  EXPECT_TRUE(WasRenderedResolution(format_vga_));
  // Now request restart with qvga.
  EXPECT_TRUE(capture_manager_.RestartVideoCapture(
      &video_capturer_, format_vga_, format_qvga_,
      cricket::CaptureManager::kRequestRestart));
  EXPECT_TRUE(video_capturer_.CaptureFrame());
  EXPECT_EQ(2, NumFramesRendered());
  EXPECT_TRUE(WasRenderedResolution(format_vga_));
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                format_qvga_));
}
