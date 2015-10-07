/*
 * libjingle
 * Copyright 2010 Google Inc.
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

// If we don't have a WebRtcVideoFrame, just skip all of these tests.
#if defined(HAVE_WEBRTC_VIDEO)
#include <limits.h>  // For INT_MAX
#include <string>
#include <vector>

#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/testutils.h"
#include "talk/media/base/videoadapter.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

namespace {
static const uint32_t kWaitTimeout = 3000U;       // 3 seconds.
static const uint32_t kShortWaitTimeout = 1000U;  // 1 second.
  void UpdateCpuLoad(CoordinatedVideoAdapter* adapter,
    int current_cpus, int max_cpus, float process_load, float system_load) {
    adapter->set_cpu_load_min_samples(1);
    adapter->OnCpuLoadUpdated(current_cpus, max_cpus,
                              process_load, system_load);
  }
}

class VideoAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    capturer_.reset(new FakeVideoCapturer);
    capture_format_ = capturer_->GetSupportedFormats()->at(0);
    capture_format_.interval = VideoFormat::FpsToInterval(50);
    adapter_.reset(new VideoAdapter());
    adapter_->SetInputFormat(capture_format_);

    listener_.reset(new VideoCapturerListener(adapter_.get()));
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

  class CpuAdapterListener: public sigslot::has_slots<> {
   public:
    CpuAdapterListener() : received_cpu_signal_(false) {}
    void OnCpuAdaptationSignalled() { received_cpu_signal_ = true; }
    bool received_cpu_signal() { return received_cpu_signal_; }
   private:
    bool received_cpu_signal_;
  };

  void VerifyAdaptedResolution(const VideoCapturerListener::Stats& stats,
                               int width,
                               int height) {
    EXPECT_EQ(width, stats.adapted_width);
    EXPECT_EQ(height, stats.adapted_height);
  }

  rtc::scoped_ptr<FakeVideoCapturer> capturer_;
  rtc::scoped_ptr<VideoAdapter> adapter_;
  rtc::scoped_ptr<VideoCapturerListener> listener_;
  VideoFormat capture_format_;
};


// Test adapter remembers exact pixel count
TEST_F(VideoAdapterTest, AdaptNumPixels) {
  adapter_->SetOutputNumPixels(123456);
  EXPECT_EQ(123456, adapter_->GetOutputNumPixels());
}

// Test adapter is constructed but not activated. Expect no frame drop and no
// resolution change.
TEST_F(VideoAdapterTest, AdaptInactive) {
  // Output resolution is not set.
  EXPECT_EQ(INT_MAX, adapter_->GetOutputNumPixels());

  // Call Adapter with some frames.
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height);
}

// Do not adapt the frame rate or the resolution. Expect no frame drop and no
// resolution change.
TEST_F(VideoAdapterTest, AdaptNothing) {
  adapter_->SetOutputFormat(capture_format_);
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
  adapter_->SetInputFormat(format);
  adapter_->SetOutputFormat(format);
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
  adapter_->SetOutputFormat(request_format);
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
  adapter_->SetOutputFormat(request_format);
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
  adapter_->SetOutputFormat(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop before adaptation.
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  // Adapat the frame rate.
  request_format.interval *= 2;
  adapter_->SetOutputFormat(request_format);

  for (int i = 0; i < 20; ++i)
    capturer_->CaptureFrame();

  // Verify frame drop after adaptation.
  EXPECT_GT(listener_->GetStats().dropped_frames, 0);
}

// Set a very high output pixel resolution. Expect no resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionHighLimit) {
  adapter_->SetOutputNumPixels(INT_MAX);
  VideoFormat adapted_format = adapter_->AdaptFrameResolution(
      capture_format_.width, capture_format_.height);
  EXPECT_EQ(capture_format_.width, adapted_format.width);
  EXPECT_EQ(capture_format_.height, adapted_format.height);

  adapter_->SetOutputNumPixels(987654321);
  adapted_format = capture_format_,
  adapter_->AdaptFrameResolution(capture_format_.width, capture_format_.height);
  EXPECT_EQ(capture_format_.width, adapted_format.width);
  EXPECT_EQ(capture_format_.height, adapted_format.height);
}

// Adapt the frame resolution to be the same as capture resolution. Expect no
// resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionIdentical) {
  adapter_->SetOutputFormat(capture_format_);
  const VideoFormat adapted_format = adapter_->AdaptFrameResolution(
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
  adapter_->SetOutputFormat(request_format);
  const VideoFormat adapted_format = adapter_->AdaptFrameResolution(
      request_format.width, request_format.height);
  EXPECT_EQ(request_format.width, adapted_format.width);
  EXPECT_EQ(request_format.height, adapted_format.height);
}

// Adapt the pixel resolution to 0. Expect frame drop.
TEST_F(VideoAdapterTest, AdaptFrameResolutionDrop) {
  adapter_->SetOutputNumPixels(0);
  EXPECT_TRUE(
      adapter_->AdaptFrameResolution(capture_format_.width,
                                     capture_format_.height).IsSize0x0());
}

// Adapt the frame resolution to be a quarter of the capture resolution at the
// beginning. Expect resolution change.
TEST_F(VideoAdapterTest, AdaptResolution) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_->SetOutputFormat(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, request_format.width, request_format.height);
}

// Adapt the frame resolution to half width. Expect resolution change.
TEST_F(VideoAdapterTest, AdaptResolutionNarrow) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  adapter_->set_scale_third(true);
  adapter_->SetOutputFormat(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change.
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width * 2 / 3,
                          capture_format_.height * 2 / 3);
}

// Adapt the frame resolution to half height. Expect resolution change.
TEST_F(VideoAdapterTest, AdaptResolutionWide) {
  VideoFormat request_format = capture_format_;
  request_format.height /= 2;
  adapter_->set_scale_third(true);
  adapter_->SetOutputFormat(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change.
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width * 2 / 3,
                          capture_format_.height * 2 / 3);
}

// Adapt the frame resolution to be a quarter of the capture resolution after
// capturing no less than 10 frames. Expect no resolution change before
// adaptation and resolution change after adaptation.
TEST_F(VideoAdapterTest, AdaptResolutionOnTheFly) {
  VideoFormat request_format = capture_format_;
  adapter_->SetOutputFormat(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no resolution change before adaptation.
  VerifyAdaptedResolution(
      listener_->GetStats(), request_format.width, request_format.height);

  // Adapt the frame resolution.
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_->SetOutputFormat(request_format);
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change after adaptation.
  VerifyAdaptedResolution(
      listener_->GetStats(), request_format.width, request_format.height);
}

// Drop all frames.
TEST_F(VideoAdapterTest, DropAllFrames) {
  VideoFormat format;  // with resolution 0x0.
  adapter_->SetOutputFormat(format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify all frames are dropped.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(stats.captured_frames, stats.dropped_frames);
}

TEST(CoordinatedVideoAdapterTest, TestCoordinatedWithoutCpuAdaptation) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(false);

  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.set_scale_third(true);
  EXPECT_EQ(format, adapter.input_format());
  EXPECT_TRUE(adapter.output_format().IsSize0x0());

  // Server format request 640x400.
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Server format request 1280x720, higher than input. Adapt nothing.
  format.width = 1280;
  format.height = 720;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Cpu load is high, but cpu adaptation is disabled. Adapt nothing.
  adapter.OnCpuLoadUpdated(1, 1, 0.99f, 0.99f);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Encoder resolution request: downgrade with different size. Adapt nothing.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Encoder resolution request: downgrade.
  adapter.OnEncoderResolutionRequest(640, 400,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Encoder resolution request: downgrade. But GD off. Adapt nothing.
  adapter.set_gd_adaptation(false);
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);
  adapter.set_gd_adaptation(true);

  // Encoder resolution request: downgrade.
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request: keep. Adapt nothing.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::KEEP);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request: upgrade.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Server format request 0x0.
  format.width = 0;
  format.height = 0;
  adapter.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter.output_format().IsSize0x0());

  // Server format request 320x200.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Server format request 160x100. But view disabled. Adapt nothing.
  adapter.set_view_adaptation(false);
  format.width = 160;
  format.height = 100;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);
  adapter.set_view_adaptation(true);

  // Enable View Switch. Expect adapt down.
  adapter.set_view_switch(true);
  format.width = 160;
  format.height = 100;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(160, adapter.output_format().width);
  EXPECT_EQ(100, adapter.output_format().height);

  // Encoder resolution request: upgrade. Adapt nothing.
  adapter.OnEncoderResolutionRequest(160, 100,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(160, adapter.output_format().width);
  EXPECT_EQ(100, adapter.output_format().height);

  // Request View of 2 / 3. Expect adapt down.
  adapter.set_view_switch(true);
  format.width = (640 * 2 + 1) / 3;
  format.height = (400 * 2 + 1) / 3;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ((640 * 2 + 1) / 3, adapter.output_format().width);
  EXPECT_EQ((400 * 2 + 1) / 3, adapter.output_format().height);


  // Request View of 3 / 8. Expect adapt down.
  adapter.set_view_switch(true);
  format.width = 640 * 3 / 8;
  format.height = 400 * 3 / 8;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640 * 3 / 8, adapter.output_format().width);
  EXPECT_EQ(400 * 3 / 8, adapter.output_format().height);

  // View Switch back up. Expect adapt.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  adapter.set_view_switch(false);

  // Encoder resolution request: upgrade. Constrained by server request.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Server format request 480x300.
  format.width = 480;
  format.height = 300;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);
}

TEST(CoordinatedVideoAdapterTest, TestCoordinatedWithCpuAdaptation) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  EXPECT_FALSE(adapter.cpu_smoothing());
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Process load is medium, but system load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.55f, 0.98f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // CPU high, but cpu adaptation disabled. Adapt nothing.
  adapter.set_cpu_adaptation(false);
  adapter.OnCpuLoadUpdated(1, 1, 0.55f, 0.98f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);
  adapter.set_cpu_adaptation(true);

  // System load is high, but time has not elaspsed. Adapt nothing.
  adapter.set_cpu_load_min_samples(2);
  adapter.OnCpuLoadUpdated(1, 1, 0.55f, 0.98f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Process load is medium, but system load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.55f, 0.98f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is CPU.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            adapter.adapt_reason());

  // Server format request 320x200. Same as CPU. Do nothing.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is CPU and VIEW.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU +
            CoordinatedVideoAdapter::ADAPTREASON_VIEW,
            adapter.adapt_reason());

  // Process load and system load are normal. Adapt nothing.
  UpdateCpuLoad(&adapter, 1, 1, 0.5f, 0.8f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Process load and system load are low, but view is still low. Adapt nothing.
  UpdateCpuLoad(&adapter, 1, 1, 0.2f, 0.3f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is VIEW.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_VIEW,
            adapter.adapt_reason());

  // Server format request 640x400. Cpu is still low.  Upgrade.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Test reason for adapting is CPU.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            adapter.adapt_reason());

  // Encoder resolution request: downgrade.
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is BANDWIDTH.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            adapter.adapt_reason());

  // Process load and system load are low. Constrained by GD. Adapt nothing
  adapter.OnCpuLoadUpdated(1, 1, 0.2f, 0.3f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request: upgrade.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Encoder resolution request: upgrade. Constrained by CPU.
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Server format request 640x400. Constrained by CPU.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);
}

TEST(CoordinatedVideoAdapterTest, TestCoordinatedWithCpuRequest) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  EXPECT_FALSE(adapter.cpu_smoothing());
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // CPU resolution request: downgrade.  Adapt down.
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // CPU resolution request: keep. Do nothing.
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::KEEP);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // CPU resolution request: downgrade, but cpu adaptation disabled.
  // Adapt nothing.
  adapter.set_cpu_adaptation(false);
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // CPU resolution request: downgrade.  Adapt down.
  adapter.set_cpu_adaptation(true);
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is CPU.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            adapter.adapt_reason());

  // CPU resolution request: downgrade, but already at minimum.  Do nothing.
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Server format request 320x200. Same as CPU. Do nothing.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is CPU and VIEW.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU +
            CoordinatedVideoAdapter::ADAPTREASON_VIEW,
            adapter.adapt_reason());

  // CPU resolution request: upgrade, but view request still low. Do nothing.
  adapter.OnCpuResolutionRequest(CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is VIEW.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_VIEW,
            adapter.adapt_reason());

  // Server format request 640x400. Cpu is still low.  Upgrade.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Test reason for adapting is CPU.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU,
            adapter.adapt_reason());

  // Encoder resolution request: downgrade.
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Test reason for adapting is BANDWIDTH.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            adapter.adapt_reason());

  // Process load and system load are low. Constrained by GD. Adapt nothing
  adapter.OnCpuLoadUpdated(1, 1, 0.2f, 0.3f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request: upgrade.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Encoder resolution request: upgrade. Constrained by CPU.
  adapter.OnEncoderResolutionRequest(480, 300,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Server format request 640x400. Constrained by CPU.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);
}

TEST(CoordinatedVideoAdapterTest, TestViewRequestPlusCameraSwitch) {
  CoordinatedVideoAdapter adapter;
  adapter.set_view_switch(true);

  // Start at HD.
  VideoFormat format(1280, 720, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  EXPECT_EQ(format, adapter.input_format());
  EXPECT_TRUE(adapter.output_format().IsSize0x0());

  // View request for VGA.
  format.width = 640;
  format.height = 360;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_VIEW, adapter.adapt_reason());

  // Now, the camera reopens at VGA.
  // Both the frame and the output format should be 640x360.
  const VideoFormat out_format = adapter.AdaptFrameResolution(640, 360);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);
  // At this point, the view is no longer adapted, since the input has resized
  // small enough to fit the last view request.
  EXPECT_EQ(0, adapter.adapt_reason());

  // And another view request comes in for 640x360, which should have no
  // real impact.
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);
  EXPECT_EQ(0, adapter.adapt_reason());
}

TEST(CoordinatedVideoAdapterTest, TestVGAWidth) {
  CoordinatedVideoAdapter adapter;
  adapter.set_view_switch(true);

  // Start at 640x480, for cameras that don't support 640x360.
  VideoFormat format(640, 480, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  EXPECT_EQ(format, adapter.input_format());
  EXPECT_TRUE(adapter.output_format().IsSize0x0());

  // Output format is 640x360, though.
  format.width = 640;
  format.height = 360;
  adapter.SetOutputFormat(format);

  // And also a view request comes for 640x360.
  adapter.OnOutputFormatRequest(format);
  // At this point, we have to adapt down to something lower.
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);

  // But if frames come in at 640x360, we shouldn't adapt them down.
  // Fake a 640x360 frame.
  VideoFormat out_format = adapter.AdaptFrameResolution(640, 360);
  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);

  // Similarly, no-op adapt requests for other reasons shouldn't change
  // adaptation state (before a previous bug, the previous EXPECTs would
  // fail and the following would succeed, as the no-op CPU request would
  // fix the adaptation state).
  adapter.set_cpu_adaptation(true);
  UpdateCpuLoad(&adapter, 1, 1, 0.7f, 0.7f);
  out_format = adapter.AdaptFrameResolution(640, 360);

  EXPECT_EQ(640, out_format.width);
  EXPECT_EQ(360, out_format.height);
}

// When adapting resolution for CPU or GD, the quantity of pixels that the
// request is based on is reduced to half or double, and then an actual
// resolution is snapped to, rounding to the closest actual resolution.
// This works well for some tolerance to 3/4, odd widths and aspect ratios
// that dont exactly match, but is not best behavior for ViewRequests which
// need to be be strictly respected to avoid going over the resolution budget
// given to the codec - 854x480 total pixels.
// ViewRequest must find a lower resolution.
TEST(CoordinatedVideoAdapterTest, TestCoordinatedViewRequestDown) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(false);

  VideoFormat format(960, 540, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.set_scale_third(true);
  EXPECT_EQ(format, adapter.input_format());
  EXPECT_TRUE(adapter.output_format().IsSize0x0());

  // Server format request 640x400. Expect HVGA.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);

  // Test reason for adapting is VIEW.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_VIEW, adapter.adapt_reason());
}

// Test that we downgrade video for cpu up to two times.
TEST(CoordinatedVideoAdapterTest, TestCpuDowngradeTimes) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  EXPECT_FALSE(adapter.cpu_smoothing());
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Process load and system load are low. Do not change the cpu desired format
  // and do not adapt.
  adapter.OnCpuLoadUpdated(1, 1, 0.2f, 0.3f);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // System load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // System load is high. Downgrade again.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // System load is still high. Do not downgrade any more.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Process load and system load are low. Upgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.2f, 0.3f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // System load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // System load is still high. Do not downgrade any more.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);
}

// Test that we respect CPU adapter threshold values.
TEST(CoordinatedVideoAdapterTest, TestAdapterCpuThreshold) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  EXPECT_FALSE(adapter.cpu_smoothing());
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Process load and system load are low. Do not change the cpu desired format
  // and do not adapt.
  adapter.OnCpuLoadUpdated(1, 1, 0.2f, 0.3f);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // System load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Test reason for adapting is CPU.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_CPU, adapter.adapt_reason());

  // System load is high. Normally downgrade but threshold is high. Do nothing.
  adapter.set_high_system_threshold(0.98f);  // Set threshold high.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // System load is medium. Normally do nothing, threshold is low. Adapt down.
  adapter.set_high_system_threshold(0.75f);  // Set threshold low.
  UpdateCpuLoad(&adapter, 1, 1, 0.8f, 0.8f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);
}


// Test that for an upgrade cpu request, we actually upgrade the desired format;
// for a downgrade request, we downgrade from the output format.
TEST(CoordinatedVideoAdapterTest, TestRealCpuUpgrade) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  adapter.set_cpu_smoothing(true);
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Process load and system load are low. Do not change the cpu desired format
  // and do not adapt.
  UpdateCpuLoad(&adapter, 1, 1, 0.2f, 0.3f);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Server format request 320x200.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Process load and system load are low. Do not change the cpu desired format
  // and do not adapt.
  UpdateCpuLoad(&adapter, 1, 1, 0.2f, 0.3f);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Server format request 640x400. Set to 640x400 immediately.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Server format request 320x200.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Process load is high, but system is not. Do not change the cpu desired
  // format and do not adapt.
  for (size_t i = 0; i < 10; ++i) {
    UpdateCpuLoad(&adapter, 1, 1, 0.75f, 0.8f);
  }
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);
}

// Test that for an upgrade encoder request, we actually upgrade the desired
//  format; for a downgrade request, we downgrade from the output format.
TEST(CoordinatedVideoAdapterTest, TestRealEncoderUpgrade) {
  CoordinatedVideoAdapter adapter;
  adapter.set_cpu_adaptation(true);
  adapter.set_cpu_smoothing(true);
  VideoFormat format(640, 400, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  // Server format request 640x400.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Encoder resolution request. Do not change the encoder desired format and
  // do not adapt.
  adapter.OnEncoderResolutionRequest(640, 400,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(400, adapter.output_format().height);

  // Server format request 320x200.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request. Do not change the encoder desired format and
  // do not adapt.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::UPGRADE);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Server format request 640x400. Set to 640x400 immediately.
  format.width = 640;
  format.height = 400;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(300, adapter.output_format().height);

  // Test reason for adapting is BANDWIDTH.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_BANDWIDTH,
            adapter.adapt_reason());

  // Server format request 320x200.
  format.width = 320;
  format.height = 200;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(200, adapter.output_format().height);

  // Encoder resolution request. Downgrade from 320x200.
  adapter.OnEncoderResolutionRequest(320, 200,
                                     CoordinatedVideoAdapter::DOWNGRADE);
  EXPECT_EQ(240, adapter.output_format().width);
  EXPECT_EQ(150, adapter.output_format().height);
}

TEST(CoordinatedVideoAdapterTest, TestNormalizeOutputFormat) {
  CoordinatedVideoAdapter adapter;
  // The input format is 640x360 and the output is limited to 16:9.
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);

  format.width = 320;
  format.height = 180;
  format.interval = VideoFormat::FpsToInterval(15);
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(180, adapter.output_format().height);
  EXPECT_EQ(VideoFormat::FpsToInterval(15), adapter.output_format().interval);

  format.width = 320;
  format.height = 200;
  format.interval = VideoFormat::FpsToInterval(40);
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(180, adapter.output_format().height);
  EXPECT_EQ(VideoFormat::FpsToInterval(30), adapter.output_format().interval);

  // Test reason for adapting is VIEW. Should work even with normalization.
  EXPECT_EQ(CoordinatedVideoAdapter::ADAPTREASON_VIEW,
            adapter.adapt_reason());

  format.width = 320;
  format.height = 240;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(180, adapter.output_format().height);

  // The input format is 640x480 and the output will be 4:3.
  format.width = 640;
  format.height = 480;
  adapter.SetInputFormat(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(240, adapter.output_format().height);

  format.width = 320;
  format.height = 240;
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(240, adapter.output_format().height);

  // The input format is initialized after the output. At that time, the output
  // height is adjusted.
  format.width = 0;
  format.height = 0;
  adapter.SetInputFormat(format);

  format.width = 320;
  format.height = 240;
  format.interval = VideoFormat::FpsToInterval(30);
  adapter.OnOutputFormatRequest(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(240, adapter.output_format().height);
  EXPECT_EQ(VideoFormat::FpsToInterval(30), adapter.output_format().interval);

  format.width = 640;
  format.height = 480;
  format.interval = VideoFormat::FpsToInterval(15);
  adapter.SetInputFormat(format);
  EXPECT_EQ(320, adapter.output_format().width);
  EXPECT_EQ(240, adapter.output_format().height);
  EXPECT_EQ(VideoFormat::FpsToInterval(15), adapter.output_format().interval);
}

// Test that we downgrade video for cpu up to two times.
TEST_F(VideoAdapterTest, CpuDowngradeAndSignal) {
  CoordinatedVideoAdapter adapter;
  CpuAdapterListener cpu_listener;
  adapter.SignalCpuAdaptationUnable.connect(
      &cpu_listener, &CpuAdapterListener::OnCpuAdaptationSignalled);

  adapter.set_cpu_adaptation(true);
  EXPECT_FALSE(adapter.cpu_smoothing());
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.OnOutputFormatRequest(format);

  // System load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);

  // System load is high. Downgrade again.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);

  // System load is still high. Do not downgrade any more. Ensure we have not
  // signalled until after the cpu warning though.
  EXPECT_TRUE(!cpu_listener.received_cpu_signal());
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  EXPECT_TRUE_WAIT(cpu_listener.received_cpu_signal(), kWaitTimeout);
}

// Test that we downgrade video for cpu up to two times.
TEST_F(VideoAdapterTest, CpuDowngradeAndDontSignal) {
  CoordinatedVideoAdapter adapter;
  CpuAdapterListener cpu_listener;
  adapter.SignalCpuAdaptationUnable.connect(
      &cpu_listener, &CpuAdapterListener::OnCpuAdaptationSignalled);

  adapter.set_cpu_adaptation(true);
  adapter.set_cpu_smoothing(true);
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.OnOutputFormatRequest(format);

  // System load is high. Downgrade.
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);

  // System load is high, process is not, Do not downgrade again.
  UpdateCpuLoad(&adapter, 1, 1, 0.25f, 0.95f);

  // System load is high, process is not, Do not downgrade again and do not
  // signal.
  adapter.set_cpu_adaptation(false);
  UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  rtc::Thread::Current()->ProcessMessages(kShortWaitTimeout);
  EXPECT_TRUE(!cpu_listener.received_cpu_signal());
  adapter.set_cpu_adaptation(true);
}

// Test that we require enough time before we downgrade.
TEST_F(VideoAdapterTest, CpuMinTimeRequirement) {
  CoordinatedVideoAdapter adapter;
  CpuAdapterListener cpu_listener;
  adapter.SignalCpuAdaptationUnable.connect(
      &cpu_listener, &CpuAdapterListener::OnCpuAdaptationSignalled);

  adapter.set_cpu_adaptation(true);
  adapter.set_cpu_smoothing(true);
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.OnOutputFormatRequest(format);

  EXPECT_EQ(3, adapter.cpu_load_min_samples());
  adapter.set_cpu_load_min_samples(5);

  for (size_t i = 0; i < 4; ++i) {
    adapter.OnCpuLoadUpdated(1, 1, 1.0f, 1.0f);
    EXPECT_EQ(640, adapter.output_format().width);
    EXPECT_EQ(360, adapter.output_format().height);
  }
  // The computed cpu load should now be around 93.5%, with the coefficient of
  // 0.4 and a seed value of 0.5. That should be high enough to adapt, but it
  // isn't enough samples, so we shouldn't have adapted on any of the previous
  // samples.

  // One more sample is enough, though, once enough time has passed.
  adapter.OnCpuLoadUpdated(1, 1, 1.0f, 1.0f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(270, adapter.output_format().height);

  // Now the cpu is lower, but we still need enough samples to upgrade.
  for (size_t i = 0; i < 4; ++i) {
    adapter.OnCpuLoadUpdated(1, 1, 0.1f, 0.1f);
    EXPECT_EQ(480, adapter.output_format().width);
    EXPECT_EQ(270, adapter.output_format().height);
  }

  // One more sample is enough, once time has elapsed.
  adapter.OnCpuLoadUpdated(1, 1, 1.0f, 1.0f);
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);
}

TEST_F(VideoAdapterTest, CpuIgnoresSpikes) {
  CoordinatedVideoAdapter adapter;
  CpuAdapterListener cpu_listener;
  adapter.SignalCpuAdaptationUnable.connect(
      &cpu_listener, &CpuAdapterListener::OnCpuAdaptationSignalled);

  adapter.set_cpu_adaptation(true);
  adapter.set_cpu_smoothing(true);
  VideoFormat format(640, 360, VideoFormat::FpsToInterval(30), FOURCC_I420);
  adapter.SetInputFormat(format);
  adapter.OnOutputFormatRequest(format);

  // System load is high. Downgrade.
  for (size_t i = 0; i < 5; ++i) {
    UpdateCpuLoad(&adapter, 1, 1, 0.95f, 0.95f);
  }
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(270, adapter.output_format().height);

  // Now we're in a state where we could upgrade or downgrade, so get to a
  // steady state of about 75% cpu usage.
  for (size_t i = 0; i < 5; ++i) {
    UpdateCpuLoad(&adapter, 1, 1, 0.75f, 0.75f);
    EXPECT_EQ(480, adapter.output_format().width);
    EXPECT_EQ(270, adapter.output_format().height);
  }

  // Now, the cpu spikes for two samples, but then goes back to
  // normal. This shouldn't cause adaptation.
  UpdateCpuLoad(&adapter, 1, 1, 0.90f, 0.90f);
  UpdateCpuLoad(&adapter, 1, 1, 0.90f, 0.90f);
  EXPECT_EQ(480, adapter.output_format().width);
  EXPECT_EQ(270, adapter.output_format().height);
  // Back to the steady state for awhile.
  for (size_t i = 0; i < 5; ++i) {
    UpdateCpuLoad(&adapter, 1, 1, 0.75, 0.75);
    EXPECT_EQ(480, adapter.output_format().width);
    EXPECT_EQ(270, adapter.output_format().height);
  }

  // Now, system cpu usage is starting to drop down. But it takes a bit before
  // it gets all the way there.
  for (size_t i = 0; i < 10; ++i) {
    UpdateCpuLoad(&adapter, 1, 1, 0.5f, 0.5f);
  }
  EXPECT_EQ(640, adapter.output_format().width);
  EXPECT_EQ(360, adapter.output_format().height);
}

}  // namespace cricket
#endif  // HAVE_WEBRTC_VIDEO
