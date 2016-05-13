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

#include <memory>
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

      int cropped_width;
      int cropped_height;
      int out_width;
      int out_height;
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
      int cropped_width;
      int cropped_height;
      int out_width;
      int out_height;
      video_adapter_->AdaptFrameResolution(in_width, in_height,
                                           &cropped_width, &cropped_height,
                                           &out_width, &out_height);
      if (out_width != 0 && out_height != 0) {
        cropped_width_ = cropped_width;
        cropped_height_ = cropped_height;
        out_width_ = out_width;
        out_height_ = out_height;
        last_adapt_was_no_op_ =
            (in_width == cropped_width && in_height == cropped_height &&
             in_width == out_width && in_height == out_height);
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
      stats.cropped_width = cropped_width_;
      stats.cropped_height = cropped_height_;
      stats.out_width = out_width_;
      stats.out_height = out_height_;
      return stats;
    }

   private:
    rtc::CriticalSection crit_;
    VideoAdapter* video_adapter_;
    int cropped_width_;
    int cropped_height_;
    int out_width_;
    int out_height_;
    int captured_frames_;
    int dropped_frames_;
    bool last_adapt_was_no_op_;
  };


  void VerifyAdaptedResolution(const VideoCapturerListener::Stats& stats,
                               int cropped_width,
                               int cropped_height,
                               int out_width,
                               int out_height) {
    EXPECT_EQ(cropped_width, stats.cropped_width);
    EXPECT_EQ(cropped_height, stats.cropped_height);
    EXPECT_EQ(out_width, stats.out_width);
    EXPECT_EQ(out_height, stats.out_height);
  }

  std::unique_ptr<FakeVideoCapturer> capturer_;
  VideoAdapter adapter_;
  int cropped_width_;
  int cropped_height_;
  int out_width_;
  int out_height_;
  std::unique_ptr<VideoCapturerListener> listener_;
  VideoFormat capture_format_;
};

// Do not adapt the frame rate or the resolution. Expect no frame drop, no
// cropping, and no resolution change.
TEST_F(VideoAdapterTest, AdaptNothing) {
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
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
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
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
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
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
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
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

// Set a very high output pixel resolution. Expect no cropping or resolution
// change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionHighLimit) {
  VideoFormat output_format = capture_format_;
  output_format.width *= 10;
  output_format.height *= 10;
  adapter_.OnOutputFormatRequest(output_format);
  adapter_.AdaptFrameResolution(capture_format_.width, capture_format_.height,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(capture_format_.width, out_width_);
  EXPECT_EQ(capture_format_.height, out_height_);
}

// Adapt the frame resolution to be the same as capture resolution. Expect no
// cropping or resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionIdentical) {
  adapter_.OnOutputFormatRequest(capture_format_);
  adapter_.AdaptFrameResolution(capture_format_.width, capture_format_.height,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(capture_format_.width, out_width_);
  EXPECT_EQ(capture_format_.height, out_height_);
}

// Adapt the frame resolution to be a quarter of the capture resolution. Expect
// no cropping, but a resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionQuarter) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  adapter_.AdaptFrameResolution(capture_format_.width, capture_format_.height,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(request_format.width, out_width_);
  EXPECT_EQ(request_format.height, out_height_);
}

// Adapt the pixel resolution to 0. Expect frame drop.
TEST_F(VideoAdapterTest, AdaptFrameResolutionDrop) {
  VideoFormat output_format = capture_format_;
  output_format.width = 0;
  output_format.height = 0;
  adapter_.OnOutputFormatRequest(output_format);
  adapter_.AdaptFrameResolution(capture_format_.width, capture_format_.height,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(0, out_width_);
  EXPECT_EQ(0, out_height_);
}

// Adapt the frame resolution to be a quarter of the capture resolution at the
// beginning. Expect no cropping but a resolution change.
TEST_F(VideoAdapterTest, AdaptResolution) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop, no cropping, and resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);
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
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);

  // Adapt the frame resolution.
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change after adaptation.
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);
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
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Format request 640x400.
  format.height = 400;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Request 1280x720, higher than input, but aspect 16:9. Expect cropping but
  // no scaling.
  format.width = 1280;
  format.height = 720;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Request 0x0.
  format.width = 0;
  format.height = 0;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(0, out_width_);
  EXPECT_EQ(0, out_height_);

  // Request 320x200. Expect scaling, but no cropping.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution close to 2/3 scale. Expect adapt down. Scaling to 2/3
  // is not optimized and not allowed, therefore 1/2 scaling will be used
  // instead.
  format.width = 424;
  format.height = 265;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution of 3 / 8. Expect adapt down.
  format.width = 640 * 3 / 8;
  format.height = 400 * 3 / 8;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640 * 3 / 8, out_width_);
  EXPECT_EQ(400 * 3 / 8, out_height_);

  // Switch back up. Expect adapt.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Format request 480x300.
  format.width = 480;
  format.height = 300;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 400,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(300, out_height_);
}

TEST_F(VideoAdapterTest, TestViewRequestPlusCameraSwitch) {
  // Start at HD.
  VideoFormat format(1280, 720, VideoFormat::FpsToInterval(30), 0);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Format request for VGA.
  format.width = 640;
  format.height = 360;
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Now, the camera reopens at VGA.
  // Both the frame and the output format should be 640x360.
  adapter_.AdaptFrameResolution(640, 360,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // And another view request comes in for 640x360, which should have no
  // real impact.
  adapter_.OnOutputFormatRequest(format);
  adapter_.AdaptFrameResolution(640, 360,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestVGAWidth) {
  // Reqeuested Output format is 640x360.
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(format);

  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  // Expect cropping.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // But if frames come in at 640x360, we shouldn't adapt them down.
  adapter_.AdaptFrameResolution(640, 360,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInSmallSteps) {
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Adapt down one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(1280 * 720 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(960 * 540 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(480 * 270));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestMaxZero) {
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(0), rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(0, out_width_);
  EXPECT_EQ(0, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInLargeSteps) {
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestOnOutputFormatRequestCapsMaxResolution) {
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  VideoFormat new_format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(new_format);
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestReset) {
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(), rtc::Optional<int>());
  adapter_.AdaptFrameResolution(1280, 720,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestCroppingWithResolutionRequest) {
  // Ask for 640x360 (16:9 aspect).
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(
      VideoFormat(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420));
  // Send 640x480 (4:3 aspect).
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1),
                               rtc::Optional<int>());
  // Expect cropping to 16:9 format and 3/4 scaling.
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(480 * 270 - 1),
                               rtc::Optional<int>());
  // Expect cropping to 16:9 format and 1/2 scaling.
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(180, out_height_);

  // Adapt up one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(320 * 180));
  // Expect cropping to 16:9 format and 3/4 scaling.
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(480 * 270));
  // Expect cropping to 16:9 format and no scaling.
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Try to adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360));
  // Expect cropping to 16:9 format and no scaling.
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestCroppingOddResolution) {
  // Ask for 640x360 (16:9 aspect), with 3/16 scaling.
  adapter_.SetExpectedInputFrameInterval(VideoFormat::FpsToInterval(30));
  adapter_.OnOutputFormatRequest(
      VideoFormat(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420));
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 * 3 / 16 * 3 / 16),
                               rtc::Optional<int>());

  // Send 640x480 (4:3 aspect).
  adapter_.AdaptFrameResolution(640, 480,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);

  // Instead of getting the exact aspect ratio with cropped resolution 640x360,
  // the resolution should be adjusted to get a perfect scale factor instead.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(368, cropped_height_);
  EXPECT_EQ(120, out_width_);
  EXPECT_EQ(69, out_height_);
}

}  // namespace cricket
