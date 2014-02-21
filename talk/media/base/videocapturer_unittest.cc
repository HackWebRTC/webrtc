// Copyright 2008 Google Inc.

#include <stdio.h>
#include <vector>

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakemediaprocessor.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/fakevideorenderer.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videoprocessor.h"

// If HAS_I420_FRAME is not defined the video capturer will not be able to
// provide OnVideoFrame-callbacks since they require cricket::CapturedFrame to
// be decoded as a cricket::VideoFrame (i.e. an I420 frame). This functionality
// only exist if HAS_I420_FRAME is defined below. I420 frames are also a
// requirement for the VideoProcessors so they will not be called either.
#if defined(HAVE_WEBRTC_VIDEO)
#define HAS_I420_FRAME
#endif

using cricket::FakeVideoCapturer;

namespace {

const int kMsCallbackWait = 500;
// For HD only the height matters.
const int kMinHdHeight = 720;
const uint32 kTimeout = 5000U;

}  // namespace

// Sets the elapsed time in the video frame to 0.
class VideoProcessor0 : public cricket::VideoProcessor {
 public:
  virtual void OnFrame(uint32 /*ssrc*/, cricket::VideoFrame* frame,
                       bool* drop_frame) {
    frame->SetElapsedTime(0u);
  }
};

// Adds one to the video frame's elapsed time. Note that VideoProcessor0 and
// VideoProcessor1 are not commutative.
class VideoProcessor1 : public cricket::VideoProcessor {
 public:
  virtual void OnFrame(uint32 /*ssrc*/, cricket::VideoFrame* frame,
                       bool* drop_frame) {
    int64 elapsed_time = frame->GetElapsedTime();
    frame->SetElapsedTime(elapsed_time + 1);
  }
};

class VideoCapturerTest
    : public sigslot::has_slots<>,
      public testing::Test {
 public:
  VideoCapturerTest()
      : capture_state_(cricket::CS_STOPPED),
        num_state_changes_(0),
        video_frames_received_(0),
        last_frame_elapsed_time_(0) {
    capturer_.SignalVideoFrame.connect(this, &VideoCapturerTest::OnVideoFrame);
    capturer_.SignalStateChange.connect(this,
                                        &VideoCapturerTest::OnStateChange);
  }

 protected:
  void OnVideoFrame(cricket::VideoCapturer*, const cricket::VideoFrame* frame) {
    ++video_frames_received_;
    last_frame_elapsed_time_ = frame->GetElapsedTime();
    renderer_.RenderFrame(frame);
  }
  void OnStateChange(cricket::VideoCapturer*,
                     cricket::CaptureState capture_state) {
    capture_state_ = capture_state;
    ++num_state_changes_;
  }
  cricket::CaptureState capture_state() { return capture_state_; }
  int num_state_changes() { return num_state_changes_; }
  int video_frames_received() const {
    return video_frames_received_;
  }
  int64 last_frame_elapsed_time() const { return last_frame_elapsed_time_; }

  cricket::FakeVideoCapturer capturer_;
  cricket::CaptureState capture_state_;
  int num_state_changes_;
  int video_frames_received_;
  int64 last_frame_elapsed_time_;
  cricket::FakeVideoRenderer renderer_;
};

TEST_F(VideoCapturerTest, CaptureState) {
  EXPECT_TRUE(capturer_.enable_video_adapter());
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, num_state_changes());
  capturer_.Stop();
  EXPECT_EQ_WAIT(cricket::CS_STOPPED, capture_state(), kMsCallbackWait);
  EXPECT_EQ(2, num_state_changes());
  capturer_.Stop();
  talk_base::Thread::Current()->ProcessMessages(100);
  EXPECT_EQ(2, num_state_changes());
}

TEST_F(VideoCapturerTest, TestRestart) {
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, num_state_changes());
  EXPECT_TRUE(capturer_.Restart(cricket::VideoFormat(
      320,
      240,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_GE(1, num_state_changes());
  capturer_.Stop();
  talk_base::Thread::Current()->ProcessMessages(100);
  EXPECT_FALSE(capturer_.IsRunning());
}

TEST_F(VideoCapturerTest, TestStartingWithRestart) {
  EXPECT_FALSE(capturer_.IsRunning());
  EXPECT_TRUE(capturer_.Restart(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
}

TEST_F(VideoCapturerTest, TestRestartWithSameFormat) {
  cricket::VideoFormat format(640, 480,
                              cricket::VideoFormat::FpsToInterval(30),
                              cricket::FOURCC_I420);
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(format));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, num_state_changes());
  EXPECT_TRUE(capturer_.Restart(format));
  EXPECT_EQ(cricket::CS_RUNNING, capture_state());
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(1, num_state_changes());
}

TEST_F(VideoCapturerTest, CameraOffOnMute) {
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(0, video_frames_received());
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(1, video_frames_received());
  EXPECT_FALSE(capturer_.IsMuted());

  // Mute the camera and expect black output frame.
  capturer_.MuteToBlackThenPause(true);
  EXPECT_TRUE(capturer_.IsMuted());
  for (int i = 0; i < 31; ++i) {
    EXPECT_TRUE(capturer_.CaptureFrame());
    EXPECT_TRUE(renderer_.black_frame());
  }
  EXPECT_EQ(32, video_frames_received());
  EXPECT_EQ_WAIT(cricket::CS_PAUSED,
                 capturer_.capture_state(), kTimeout);

  // Verify that the camera is off.
  EXPECT_FALSE(capturer_.CaptureFrame());
  EXPECT_EQ(32, video_frames_received());

  // Unmute the camera and expect non-black output frame.
  capturer_.MuteToBlackThenPause(false);
  EXPECT_FALSE(capturer_.IsMuted());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING,
                 capturer_.capture_state(), kTimeout);
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_FALSE(renderer_.black_frame());
  EXPECT_EQ(33, video_frames_received());
}

TEST_F(VideoCapturerTest, ScreencastScaledMaxPixels) {
  capturer_.SetScreencast(true);

  int kWidth = 1280;
  int kHeight = 720;

  // Screencasts usually have large weird dimensions and are ARGB.
  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(kWidth, kHeight,
      cricket::VideoFormat::FpsToInterval(5), cricket::FOURCC_ARGB));
  formats.push_back(cricket::VideoFormat(2 * kWidth, 2 * kHeight,
      cricket::VideoFormat::FpsToInterval(5), cricket::FOURCC_ARGB));
  capturer_.ResetSupportedFormats(formats);


  EXPECT_EQ(0, capturer_.screencast_max_pixels());
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      2 * kWidth,
      2 * kHeight,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_ARGB)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  renderer_.SetSize(2 * kWidth, 2 * kHeight, 0);
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(1, renderer_.num_rendered_frames());

  capturer_.set_screencast_max_pixels(kWidth * kHeight);
  renderer_.SetSize(kWidth, kHeight, 0);
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(2, renderer_.num_rendered_frames());
}


TEST_F(VideoCapturerTest, TestFourccMatch) {
  cricket::VideoFormat desired(640, 480,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.fourcc = cricket::FOURCC_MJPG;
  EXPECT_FALSE(capturer_.GetBestCaptureFormat(desired, &best));

  desired.fourcc = cricket::FOURCC_I420;
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
}

TEST_F(VideoCapturerTest, TestResolutionMatch) {
  cricket::VideoFormat desired(1920, 1080,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  // Ask for 1920x1080. Get HD 1280x720 which is the highest.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(1280, best.width);
  EXPECT_EQ(720, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 360;
  desired.height = 250;
  // Ask for a little higher than QVGA. Get QVGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 480;
  desired.height = 270;
  // Ask for HVGA. Get VGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 320;
  desired.height = 240;
  // Ask for QVGA. Get QVGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 80;
  desired.height = 60;
  // Ask for lower than QQVGA. Get QQVGA, which is the lowest.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(160, best.width);
  EXPECT_EQ(120, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);
}

TEST_F(VideoCapturerTest, TestHDResolutionMatch) {
  // Add some HD formats typical of a mediocre HD webcam.
  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(960, 544,
      cricket::VideoFormat::FpsToInterval(24), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(2592, 1944,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(formats);

  cricket::VideoFormat desired(960, 720,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  // Ask for 960x720 30 fps. Get qHD 24 fps
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(960, best.width);
  EXPECT_EQ(544, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(24), best.interval);

  desired.width = 960;
  desired.height = 544;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for qHD 30 fps. Get qHD 24 fps
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(960, best.width);
  EXPECT_EQ(544, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(24), best.interval);

  desired.width = 360;
  desired.height = 250;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for a little higher than QVGA. Get QVGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 480;
  desired.height = 270;
  // Ask for HVGA. Get VGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 320;
  desired.height = 240;
  // Ask for QVGA. Get QVGA.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 160;
  desired.height = 120;
  // Ask for lower than QVGA. Get QVGA, which is the lowest.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 1280;
  desired.height = 720;
  // Ask for HD. 720p fps is too low. Get VGA which has 30 fps.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 1280;
  desired.height = 720;
  desired.interval = cricket::VideoFormat::FpsToInterval(15);
  // Ask for HD 15 fps. Fps matches. Get HD
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(1280, best.width);
  EXPECT_EQ(720, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);

  desired.width = 1920;
  desired.height = 1080;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for 1080p. Fps of HD formats is too low. Get VGA which can do 30 fps.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);
}

// Some cameras support 320x240 and 320x640. Verify we choose 320x240.
TEST_F(VideoCapturerTest, TestStrangeFormats) {
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 640,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(320, 200,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(320, 180,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }

  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(320, 640,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }
}

// Some cameras only have very low fps. Verify we choose something sensible.
TEST_F(VideoCapturerTest, TestPoorFpsFormats) {
  // all formats are low framerate
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(2), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // Increase framerate of 320x240. Expect low fps VGA avoided.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(2), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }
}

// Some cameras support same size with different frame rates. Verify we choose
// the frame rate properly.
TEST_F(VideoCapturerTest, TestSameSizeDifferentFpsFormats) {
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(20), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats = supported_formats;
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
    EXPECT_EQ(required_formats[i].interval, best.interval);
  }
}

// Some cameras support the correct resolution but at a lower fps than
// we'd like. This tests we get the expected resolution and fps.
TEST_F(VideoCapturerTest, TestFpsFormats) {
  // We have VGA but low fps. Choose VGA, not HD
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ANY));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(20), cricket::FOURCC_ANY));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_ANY));
  cricket::VideoFormat best;

  // Expect 30 fps to choose 30 fps format.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[0], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(400, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  // Expect 20 fps to choose 30 fps format.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[1], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(400, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  // Expect 10 fps to choose 15 fps format and set fps to 15.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);

  // We have VGA 60 fps and 15 fps. Choose best fps.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_MJPG));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  // Expect 30 fps to choose 60 fps format and will set best fps to 60.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[0], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(60), best.interval);

  // Expect 20 fps to choose 60 fps format, and will set best fps to 60.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[1], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(60), best.interval);

  // Expect 10 fps to choose 15 fps.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);
}

TEST_F(VideoCapturerTest, TestRequest16x10_9) {
  std::vector<cricket::VideoFormat> supported_formats;
  // We do not support HD, expect 4x3 for 4x3, 16x10, and 16x9 requests.
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats = supported_formats;
  cricket::VideoFormat best;
  // Expect 4x3, 16x10, and 16x9 requests are respected.
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // We do not support 16x9 HD, expect 4x3 for 4x3, 16x10, and 16x9 requests.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(960, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  // Expect 4x3, 16x10, and 16x9 requests are respected.
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // We support 16x9HD, Expect 4x3, 16x10, and 16x9 requests are respected.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);

  // Expect 4x3 for 4x3 and 16x10 requests.
  for (size_t i = 0; i < required_formats.size() - 1; ++i) {
    EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // Expect 16x9 for 16x9 request.
  EXPECT_TRUE(capturer_.GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(360, best.height);
}

#if defined(HAS_I420_FRAME)
TEST_F(VideoCapturerTest, VideoFrame) {
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(0, video_frames_received());
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(1, video_frames_received());
}

TEST_F(VideoCapturerTest, ProcessorChainTest) {
  VideoProcessor0 processor0;
  VideoProcessor1 processor1;
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(0, video_frames_received());
  // First processor sets elapsed time to 0.
  capturer_.AddVideoProcessor(&processor0);
  // Second processor adds 1 to the elapsed time. I.e. a frames elapsed time
  // should now always be 1 (and not 0).
  capturer_.AddVideoProcessor(&processor1);
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(1, video_frames_received());
  EXPECT_EQ(1u, last_frame_elapsed_time());
  capturer_.RemoveVideoProcessor(&processor1);
  EXPECT_TRUE(capturer_.CaptureFrame());
  // Since processor1 has been removed the elapsed time should now be 0.
  EXPECT_EQ(2, video_frames_received());
  EXPECT_EQ(0u, last_frame_elapsed_time());
}

TEST_F(VideoCapturerTest, ProcessorDropFrame) {
  cricket::FakeMediaProcessor dropping_processor_;
  dropping_processor_.set_drop_frames(true);
  EXPECT_EQ(cricket::CS_RUNNING, capturer_.Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_.IsRunning());
  EXPECT_EQ(0, video_frames_received());
  // Install a processor that always drop frames.
  capturer_.AddVideoProcessor(&dropping_processor_);
  EXPECT_TRUE(capturer_.CaptureFrame());
  EXPECT_EQ(0, video_frames_received());
}
#endif  // HAS_I420_FRAME

bool HdFormatInList(const std::vector<cricket::VideoFormat>& formats) {
  for (std::vector<cricket::VideoFormat>::const_iterator found =
           formats.begin(); found != formats.end(); ++found) {
    if (found->height >= kMinHdHeight) {
      return true;
    }
  }
  return false;
}

TEST_F(VideoCapturerTest, Whitelist) {
  // The definition of HD only applies to the height. Set the HD width to the
  // smallest legal number to document this fact in this test.
  const int kMinHdWidth = 1;
  cricket::VideoFormat hd_format(kMinHdWidth,
                                 kMinHdHeight,
                                 cricket::VideoFormat::FpsToInterval(30),
                                 cricket::FOURCC_I420);
  cricket::VideoFormat vga_format(640, 480,
                                  cricket::VideoFormat::FpsToInterval(30),
                                  cricket::FOURCC_I420);
  std::vector<cricket::VideoFormat> formats = *capturer_.GetSupportedFormats();
  formats.push_back(hd_format);

  // Enable whitelist. Expect HD not in list.
  capturer_.set_enable_camera_list(true);
  capturer_.ResetSupportedFormats(formats);
  EXPECT_TRUE(HdFormatInList(*capturer_.GetSupportedFormats()));
  capturer_.ConstrainSupportedFormats(vga_format);
  EXPECT_FALSE(HdFormatInList(*capturer_.GetSupportedFormats()));

  // Disable whitelist. Expect HD in list.
  capturer_.set_enable_camera_list(false);
  capturer_.ResetSupportedFormats(formats);
  EXPECT_TRUE(HdFormatInList(*capturer_.GetSupportedFormats()));
  capturer_.ConstrainSupportedFormats(vga_format);
  EXPECT_TRUE(HdFormatInList(*capturer_.GetSupportedFormats()));
}

TEST_F(VideoCapturerTest, BlacklistAllFormats) {
  cricket::VideoFormat vga_format(640, 480,
                                  cricket::VideoFormat::FpsToInterval(30),
                                  cricket::FOURCC_I420);
  std::vector<cricket::VideoFormat> supported_formats;
  // Mock a device that only supports HD formats.
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1920, 1080,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_.ResetSupportedFormats(supported_formats);
  EXPECT_EQ(2u, capturer_.GetSupportedFormats()->size());
  // Now, enable the list, which would exclude both formats. However, since
  // only HD formats are available, we refuse to filter at all, so we don't
  // break this camera.
  capturer_.set_enable_camera_list(true);
  capturer_.ConstrainSupportedFormats(vga_format);
  EXPECT_EQ(2u, capturer_.GetSupportedFormats()->size());
  // To make sure it's not just the camera list being broken, add in VGA and
  // try again. This time, only the VGA format should be there.
  supported_formats.push_back(vga_format);
  capturer_.ResetSupportedFormats(supported_formats);
  ASSERT_EQ(1u, capturer_.GetSupportedFormats()->size());
  EXPECT_EQ(vga_format.height, capturer_.GetSupportedFormats()->at(0).height);
}
