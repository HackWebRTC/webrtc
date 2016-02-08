/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/capturemanager.h"

#include "webrtc/base/arraysize.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/fakevideorenderer.h"

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
    for (uint32_t i = 0; i < arraysize(kCameraFormats); ++i) {
      formats.push_back(cricket::VideoFormat(kCameraFormats[i]));
    }
    video_capturer_.ResetSupportedFormats(formats);
  }
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
  cricket::FakeVideoCapturer video_capturer_;
  cricket::FakeVideoRenderer video_renderer_;

  cricket::CaptureManager capture_manager_;

  cricket::CaptureState capture_state_;
  int callback_count_;
  cricket::VideoFormat format_vga_;
  cricket::VideoFormat format_qvga_;
};

// Incorrect use cases.
TEST_F(CaptureManagerTest, InvalidAddingRemoving) {
  EXPECT_FALSE(capture_manager_.StopVideoCapture(&video_capturer_,
                                                 cricket::VideoFormat()));
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  // NULL argument currently allowed, and does nothing.
  capture_manager_.AddVideoSink(&video_capturer_, NULL);
  EXPECT_TRUE(capture_manager_.StopVideoCapture(&video_capturer_, format_vga_));
}

// Valid use cases
TEST_F(CaptureManagerTest, KeepFirstResolutionHigh) {
  EXPECT_TRUE(capture_manager_.StartVideoCapture(&video_capturer_,
                                                 format_vga_));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, callback_count());
  capture_manager_.AddVideoSink(&video_capturer_, &video_renderer_);
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
  capture_manager_.AddVideoSink(&video_capturer_, &video_renderer_);
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
  capture_manager_.AddVideoSink(&video_capturer_, &video_renderer_);
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
  capture_manager_.AddVideoSink(&video_capturer_, &video_renderer_);
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
  capture_manager_.AddVideoSink(&video_capturer_, &video_renderer_);
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
