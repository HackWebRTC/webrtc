/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>  // For INT_MAX

#include <string>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/testutils.h"
#include "webrtc/media/base/videoadapter.h"

namespace cricket {

class VideoAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    capturer_.reset(new FakeVideoCapturer);
    capture_format_ = capturer_->GetSupportedFormats()->at(0);
    capture_format_.interval = VideoFormat::FpsToInterval(50);
    adapter_.SetExpectedInputFrameInterval(capture_format_.interval);

    listener_.reset(new VideoCapturerListener(&adapter_));
    capturer_->SignalFrameCaptured.connect(
        listener_.get(), &VideoCapturerListener::OnFrameCaptured);
  }

  virtual void TearDown() {
    // Explicitly disconnect the VideoCapturer before to avoid data races
    // (frames delivered to VideoCapturerListener while it's being destructed).
    capturer_->SignalFrameCaptured.disconnect_all();
  }

 protected:
  class VideoCapturerListener: public sigslot::has_slots<> {
   public:
    struct Stats {
      int captured_frames;
      int dropped_frames;
      bool last_adapt_was_no_op;

      int adapted_width;
      int adapted_height;
    };

    explicit VideoCapturerListener(VideoAdapter* adapter)
        : video_adapter_(adapter),
          captured_frames_(0),
          dropped_frames_(0),
          last_adapt_was_no_op_(false) {
    }

    void OnFrameCaptured(VideoCapturer* capturer,
                         const CapturedFrame* captured_frame) {
      rtc::CritScope lock(&crit_);
      const int in_width = captured_frame->width;
      const int in_height = abs(captured_frame->height);
      const VideoFormat adapted_format =
          video_adapter_->AdaptFrameResolution(in_width, in_height);
      if (!adapted_format.IsSize0x0()) {
        adapted_format_ = adapted_format;
        last_adapt_was_no_op_ = (in_width == adapted_format.width &&
                                 in_height == adapted_format.height);
      } else {
        ++dropped_frames_;
      }
      ++captured_frames_;
    }

    Stats GetStats() {
      rtc::CritScope lock(&crit_);
      Stats stats;
      stats.captured_frames = captured_frames_;
      stats.dropped_frames = dropped_frames_;
      stats.last_adapt_was_no_op = last_adapt_was_no_op_;
      if (!adapted_format_.IsSize0x0()) {
        stats.adapted_width = adapted_format_.width;
        stats.adapted_height = adapted_format_.height;
      } else {
        stats.adapted_width = stats.adapted_height = -1;
      }

      return stats;
    }

   private:
    rtc::CriticalSection crit_;
    VideoAdapter* video_adapter_;
    VideoFormat adapted_format_;
    int captured_frames_;
    int dropped_frames_;
    bool last_adapt_was_no_op_;
  };


  void VerifyAdaptedResolution(const VideoCapturerListener::Stats& stats,
                               int width,
                               int height) {
    EXPECT_EQ(width, stats.adapted_width);
    EXPECT_EQ(height, stats.adapted_height);
  }

  std::unique_ptr<FakeVideoCapturer> capturer_;
  VideoAdapter adapter_;
  std::unique_ptr<VideoCapturerListener> listener_;
  VideoFormat capture_format_;
};

// Do not adapt the frame rate or the resolution. Expect no frame drop and no
// resolution change.
TEST_F(VideoAdapterTest, AdaptNothing) {
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height);
  EXPECT_TRUE(stats.last_adapt_was_no_op);
}

TEST_F(VideoAdapterTest, AdaptZeroInterval) {
  VideoFormat format = capturer_->GetSupportedFormats()->at(0);
  format.interval = 0;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no crash and that frames aren't dropped.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height);
}

// Adapt the frame rate to be half of the capture rate at the beginning. Expect
// the number of dropped frames to be half of the number the captured frames.
TEST_F(VideoAdapterTest, AdaptFramerate) {
  VideoFormat request_format = capture_format_;
  request_format.interval *= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(stats.captured_frames / 2, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height);
}

// Adapt the frame rate to be half of the capture rate at the beginning. Expect
// the number of dropped frames to be half of the number the captured frames.
TEST_F(VideoAdapterTest, AdaptFramerateVariable) {
  VideoFormat request_format = capture_format_;
  request_format.interval = request_format.interval * 3 / 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 30; ++i)
    capturer_->CaptureFrame();

  // Verify frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 30);
  // Verify 2 / 3 kept (20) and 1 / 3 dropped (10).
  EXPECT_EQ(stats.captured_frames * 1 / 3, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height);
}

// Adapt the frame rate to be half of the capture rate after capturing no less
// than 10 frames. Expect no frame dropped before adaptation and frame dropped
// after adaptation.
TEST_F(VideoAdapterTest, AdaptFramerateOntheFly) {
  VideoFormat request_format = capture_format_;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop before adaptation.
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  // Adapat the frame rate.
  request_format.interval *= 2;
  adapter_.OnOutputFormatRequest(request_format);

  for (int i = 0; i < 20; ++i)
    capturer_->CaptureFrame();

  // Verify frame drop after adaptation.
  EXPECT_GT(listener_->GetStats().dropped_frames, 0);
}

// Set a very high output pixel resolution. Expect no resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionHighLimit) {
  VideoFormat output_format = capture_format_;
  output_format.width = 2560;
  output_format.height = 2560;
  adapter_.OnOutputFormatRequest(output_format);
  VideoFormat adapted_format = adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height);
  EXPECT_EQ(capture_format_.width, adapted_format.width);
  EXPECT_EQ(capture_format_.height, adapted_format.height);
}

// Adapt the frame resolution to be the same as capture resolution. Expect no
// resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionIdentical) {
  adapter_.OnOutputFormatRequest(capture_format_);
  const VideoFormat adapted_format = adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height);
  EXPECT_EQ(capture_format_.width, adapted_format.width);
  EXPECT_EQ(capture_format_.height, adapted_format.height);
}

// Adapt the frame resolution to be a quarter of the capture resolution. Expect
// resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionQuarter) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  const VideoFormat adapted_format = adapter_.AdaptFrameResolution(
      request_format.width, request_format.height);
  EXPECT_EQ(request_format.width, adapted_format.width);
  EXPECT_EQ(request_format.height, adapted_format.height);
}

// Adapt the pixel resolution to 0. Expect frame drop.
TEST_F(VideoAdapterTest, AdaptFrameResolutionDrop) {
  VideoFormat output_format = capture_format_;
  output_format.width = 0;
  output_format.height = 0;
  adapter_.OnOutputFormatRequest(output_format);
  EXPECT_TRUE(
      adapter_
          .AdaptFrameResolution(capture_format_.width, capture_format_.height)
          .IsSize0x0());
}

// Adapt the frame resolution to be a quarter of the capture resolution at the
// beginning. Expect resolution change.
TEST_F(VideoAdapterTest, AdaptResolution) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, request_format.width, request_format.height);
}

// Adapt the frame resolution to be a quarter of the capture resolution after
// capturing no less than 10 frames. Expect no resolution change before
// adaptation and resolution change after adaptation.
TEST_F(VideoAdapterTest, AdaptResolutionOnTheFly) {
  VideoFormat request_format = capture_format_;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no resolution change before adaptation.
  VerifyAdaptedResolution(
      listener_->GetStats(), request_format.width, request_format.height);

  // Adapt the frame resolution.
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change after adaptation.
  VerifyAdaptedResolution(
      listener_->GetStats(), request_format.width, request_format.height);
}

// Drop all frames.
TEST_F(VideoAdapterTest, DropAllFrames) {
  VideoFormat format;  // with resolution 0x0.
  adapter_.OnOutputFormatRequest(format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify all frames are dropped.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(stats.captured_frames, stats.dropped_frames);
}

TEST_F(VideoAdapterTest, TestOnOutputFormatRequest) {
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), 0);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  VideoFormat out_format =
      adapter_.AdaptFrameResolution(format.width, format.height);
  EXPECT_EQ(format, adapter_.input_format());
  EXPECT_EQ(format, out_format);

  // Format request 640x400.
  format.height = 400;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(400, out_format.height);

  // Request 1280x720, higher than input. Adapt nothing.
  format.width = 1280;
  format.height = 720;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(400, out_format.height);

  // Request 0x0.
  format.width = 0;
  format.height = 0;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_TRUE(out_format.IsSize0x0());

  // Request 320x200.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(320, out_format.width);
  EXPECT_EQ(200, out_format.height);

  // Request resolution of 2 / 3. Expect adapt down. Scaling to 1/3 is not
  // optimized and not allowed.
  format.width = (640 * 2 + 1) / 3;
  format.height = (400 * 2 + 1) / 3;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(320, out_format.width);
  EXPECT_EQ(200, out_format.height);

  // Request resolution of 3 / 8. Expect adapt down.
  format.width = 640 * 3 / 8;
  format.height = 400 * 3 / 8;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(640 * 3 / 8, out_format.width);
  EXPECT_EQ(400 * 3 / 8, out_format.height);

  // Switch back up. Expect adapt.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(320, out_format.width);
  EXPECT_EQ(200, out_format.height);

  // Format request 480x300.
  format.width = 480;
  format.height = 300;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 400);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(300, out_format.height);
}

TEST_F(VideoAdapterTest, TestViewRequestPlusCameraSwitch) {
  // Start at HD.
  VideoFormat format(1280, 720, VideoFormat::FpsToInterval(30), 0);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  VideoFormat out_format =
      adapter_.AdaptFrameResolution(format.width, format.height);
  EXPECT_EQ(format, adapter_.input_format());
  EXPECT_EQ(out_format, adapter_.input_format());

  // Format request for VGA.
  format.width = 640;
  format.height = 360;
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // Now, the camera reopens at VGA.
  // Both the frame and the output format should be 640x360.
  out_format = adapter_.AdaptFrameResolution(640, 360);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // And another view request comes in for 640x360, which should have no
  // real impact.
  adapter_.OnOutputFormatRequest(format);
  out_format = adapter_.AdaptFrameResolution(640, 360);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);
}

TEST_F(VideoAdapterTest, TestVGAWidth) {
  // Reqeuested Output format is 640x360.
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(format);

  VideoFormat out_format = adapter_.AdaptFrameResolution(640, 480);
  // At this point, we have to adapt down to something lower.
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // But if frames come in at 640x360, we shouldn't adapt them down.
  out_format = adapter_.AdaptFrameResolution(640, 360);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  out_format = adapter_.AdaptFrameResolution(640, 480);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(360, out_format.height);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInSmallSteps) {
  VideoFormat out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);

  // Adapt down one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(1280 * 720 - 1),
                               rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(960, out_format.width);
  EXPECT_EQ(540, out_format.height);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(960 * 540 - 1),
                               rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(270, out_format.height);

  // Adapt up one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(480 * 270));
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360));
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(960, out_format.width);
  EXPECT_EQ(540, out_format.height);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestMaxZero) {
  VideoFormat out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);

  adapter_.OnResolutionRequest(rtc::Optional<int>(0), rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(0, out_format.width);
  EXPECT_EQ(0, out_format.height);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInLargeSteps) {
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  VideoFormat out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(270, out_format.height);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);
}

TEST_F(VideoAdapterTest, TestOnOutputFormatRequestCapsMaxResolution) {
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  VideoFormat out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(270, out_format.height);

  VideoFormat new_format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(new_format);
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(270, out_format.height);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestReset) {
  VideoFormat out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);

  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(480, out_format.width);
  EXPECT_EQ(270, out_format.height);

  adapter_.OnResolutionRequest(rtc::Optional<int>(), rtc::Optional<int>());
  out_format = adapter_.AdaptFrameResolution(1280, 720);
  EXPECT_EQ(1280, out_format.width);
  EXPECT_EQ(720, out_format.height);
}

}  // namespace cricket
