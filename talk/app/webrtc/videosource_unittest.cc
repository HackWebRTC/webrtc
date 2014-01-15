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

#include <string>
#include <vector>

#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/remotevideocapturer.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakevideorenderer.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/session/media/channelmanager.h"

using webrtc::FakeConstraints;
using webrtc::VideoSource;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;
using webrtc::ObserverInterface;
using webrtc::VideoSourceInterface;

namespace {

// Max wait time for a test.
const int kMaxWaitMs = 100;

}  // anonymous namespace


// TestVideoCapturer extends cricket::FakeVideoCapturer so it can be used for
// testing without known camera formats.
// It keeps its own lists of cricket::VideoFormats for the unit tests in this
// file.
class TestVideoCapturer : public cricket::FakeVideoCapturer {
 public:
  TestVideoCapturer() : test_without_formats_(false) {
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 400,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(352, 288,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  }

  // This function is used for resetting the supported capture formats and
  // simulating a cricket::VideoCapturer implementation that don't support
  // capture format enumeration. This is used to simulate the current
  // Chrome implementation.
  void TestWithoutCameraFormats() {
    test_without_formats_ = true;
    std::vector<cricket::VideoFormat> formats;
    ResetSupportedFormats(formats);
  }

  virtual cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) {
    if (test_without_formats_) {
      std::vector<cricket::VideoFormat> formats;
      formats.push_back(capture_format);
      ResetSupportedFormats(formats);
    }
    return FakeVideoCapturer::Start(capture_format);
  }

  virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                    cricket::VideoFormat* best_format) {
    if (test_without_formats_) {
      *best_format = desired;
      return true;
    }
    return FakeVideoCapturer::GetBestCaptureFormat(desired,
                                                   best_format);
  }

 private:
  bool test_without_formats_;
};

class StateObserver : public ObserverInterface {
 public:
  explicit StateObserver(VideoSourceInterface* source)
     : state_(source->state()),
       source_(source) {
  }
  virtual void OnChanged() {
    state_ = source_->state();
  }
  MediaSourceInterface::SourceState state() const { return state_; }

 private:
  MediaSourceInterface::SourceState state_;
  talk_base::scoped_refptr<VideoSourceInterface> source_;
};

class VideoSourceTest : public testing::Test {
 protected:
  VideoSourceTest()
      : capturer_cleanup_(new TestVideoCapturer()),
        capturer_(capturer_cleanup_.get()),
        channel_manager_(new cricket::ChannelManager(
          new cricket::FakeMediaEngine(),
          new cricket::FakeDeviceManager(), talk_base::Thread::Current())) {
  }

  void SetUp() {
    ASSERT_TRUE(channel_manager_->Init());
  }

  void CreateVideoSource() {
    CreateVideoSource(NULL);
  }

  void CreateVideoSource(
      const webrtc::MediaConstraintsInterface* constraints) {
    // VideoSource take ownership of |capturer_|
    source_ = VideoSource::Create(channel_manager_.get(),
                                  capturer_cleanup_.release(),
                                  constraints);

    ASSERT_TRUE(source_.get() != NULL);
    EXPECT_EQ(capturer_, source_->GetVideoCapturer());

    state_observer_.reset(new StateObserver(source_));
    source_->RegisterObserver(state_observer_.get());
    source_->AddSink(&renderer_);
  }

  talk_base::scoped_ptr<TestVideoCapturer> capturer_cleanup_;
  TestVideoCapturer* capturer_;
  cricket::FakeVideoRenderer renderer_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  talk_base::scoped_ptr<StateObserver> state_observer_;
  talk_base::scoped_refptr<VideoSource> source_;
};


// Test that a VideoSource transition to kLive state when the capture
// device have started and kEnded if it is stopped.
// It also test that an output can receive video frames.
TEST_F(VideoSourceTest, StartStop) {
  // Initialize without constraints.
  CreateVideoSource();
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);

  ASSERT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(1, renderer_.num_rendered_frames());

  capturer_->Stop();
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
}

// Test start stop with a remote VideoSource - the video source that has a
// RemoteVideoCapturer and takes video frames from FrameInput.
TEST_F(VideoSourceTest, StartStopRemote) {
  source_ = VideoSource::Create(channel_manager_.get(),
                                new webrtc::RemoteVideoCapturer(),
                                NULL);

  ASSERT_TRUE(source_.get() != NULL);
  EXPECT_TRUE(NULL != source_->GetVideoCapturer());

  state_observer_.reset(new StateObserver(source_));
  source_->RegisterObserver(state_observer_.get());
  source_->AddSink(&renderer_);

  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);

  cricket::VideoRenderer* frameinput = source_->FrameInput();
  cricket::WebRtcVideoFrame test_frame;
  frameinput->SetSize(1280, 720, 0);
  frameinput->RenderFrame(&test_frame);
  EXPECT_EQ(1, renderer_.num_rendered_frames());

  source_->GetVideoCapturer()->Stop();
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
}

// Test that a VideoSource transition to kEnded if the capture device
// fails.
TEST_F(VideoSourceTest, CameraFailed) {
  CreateVideoSource();
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);

  capturer_->SignalStateChange(capturer_, cricket::CS_FAILED);
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
}

// Test that the capture output is CIF if we set max constraints to CIF.
// and the capture device support CIF.
TEST_F(VideoSourceTest, MandatoryConstraintCif5Fps) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMaxWidth, 352);
  constraints.AddMandatory(MediaConstraintsInterface::kMaxHeight, 288);
  constraints.AddMandatory(MediaConstraintsInterface::kMaxFrameRate, 5);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(352, format->width);
  EXPECT_EQ(288, format->height);
  EXPECT_EQ(30, format->framerate());
}

// Test that the capture output is 720P if the camera support it and the
// optional constraint is set to 720P.
TEST_F(VideoSourceTest, MandatoryMinVgaOptional720P) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMinWidth, 640);
  constraints.AddMandatory(MediaConstraintsInterface::kMinHeight, 480);
  constraints.AddOptional(MediaConstraintsInterface::kMinWidth, 1280);
  constraints.AddOptional(MediaConstraintsInterface::kMinAspectRatio,
                          1280.0 / 720);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(1280, format->width);
  EXPECT_EQ(720, format->height);
  EXPECT_EQ(30, format->framerate());
}

// Test that the capture output have aspect ratio 4:3 if a mandatory constraint
// require it even if an optional constraint request a higher resolution
// that don't have this aspect ratio.
TEST_F(VideoSourceTest, MandatoryAspectRatio4To3) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMinWidth, 640);
  constraints.AddMandatory(MediaConstraintsInterface::kMinHeight, 480);
  constraints.AddMandatory(MediaConstraintsInterface::kMaxAspectRatio,
                           640.0 / 480);
  constraints.AddOptional(MediaConstraintsInterface::kMinWidth, 1280);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(640, format->width);
  EXPECT_EQ(480, format->height);
  EXPECT_EQ(30, format->framerate());
}


// Test that the source state transition to kEnded if the mandatory aspect ratio
// is set higher than supported.
TEST_F(VideoSourceTest, MandatoryAspectRatioTooHigh) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMinAspectRatio, 2);
  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
}

// Test that the source ignores an optional aspect ratio that is higher than
// supported.
TEST_F(VideoSourceTest, OptionalAspectRatioTooHigh) {
  FakeConstraints constraints;
  constraints.AddOptional(MediaConstraintsInterface::kMinAspectRatio, 2);
  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  double aspect_ratio = static_cast<double>(format->width) / format->height;
  EXPECT_LT(aspect_ratio, 2);
}

// Test that the source starts video with the default resolution if the
// camera doesn't support capability enumeration and there are no constraints.
TEST_F(VideoSourceTest, NoCameraCapability) {
  capturer_->TestWithoutCameraFormats();

  CreateVideoSource();
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(640, format->width);
  EXPECT_EQ(480, format->height);
  EXPECT_EQ(30, format->framerate());
}

// Test that the source can start the video and get the requested aspect ratio
// if the camera doesn't support capability enumeration and the aspect ratio is
// set.
TEST_F(VideoSourceTest, NoCameraCapability16To9Ratio) {
  capturer_->TestWithoutCameraFormats();

  FakeConstraints constraints;
  double requested_aspect_ratio = 640.0 / 360;
  constraints.AddMandatory(MediaConstraintsInterface::kMinWidth, 640);
  constraints.AddMandatory(MediaConstraintsInterface::kMinAspectRatio,
                           requested_aspect_ratio);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  double aspect_ratio = static_cast<double>(format->width) / format->height;
  EXPECT_LE(requested_aspect_ratio, aspect_ratio);
}

// Test that the source state transitions to kEnded if an unknown mandatory
// constraint is found.
TEST_F(VideoSourceTest, InvalidMandatoryConstraint) {
  FakeConstraints constraints;
  constraints.AddMandatory("weird key", 640);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
}

// Test that the source ignores an unknown optional constraint.
TEST_F(VideoSourceTest, InvalidOptionalConstraint) {
  FakeConstraints constraints;
  constraints.AddOptional("weird key", 640);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
}

TEST_F(VideoSourceTest, SetValidOptionValues) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kNoiseReduction, "false");
  constraints.AddMandatory(
      MediaConstraintsInterface::kTemporalLayeredScreencast, "false");
  constraints.AddOptional(
      MediaConstraintsInterface::kLeakyBucket, "true");
  constraints.AddOptional(
      MediaConstraintsInterface::kCpuOveruseDetection, "true");

  CreateVideoSource(&constraints);

  bool value = true;
  EXPECT_TRUE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_FALSE(value);
  EXPECT_TRUE(source_->options()->
      video_temporal_layer_screencast.Get(&value));
  EXPECT_FALSE(value);
  EXPECT_TRUE(source_->options()->video_leaky_bucket.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(source_->options()->
      cpu_overuse_detection.GetWithDefaultIfUnset(false));
}

TEST_F(VideoSourceTest, OptionNotSet) {
  FakeConstraints constraints;
  CreateVideoSource(&constraints);
  bool value;
  EXPECT_FALSE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_FALSE(source_->options()->cpu_overuse_detection.Get(&value));
}

TEST_F(VideoSourceTest, MandatoryOptionOverridesOptional) {
  FakeConstraints constraints;
  constraints.AddMandatory(
      MediaConstraintsInterface::kNoiseReduction, true);
  constraints.AddOptional(
      MediaConstraintsInterface::kNoiseReduction, false);

  CreateVideoSource(&constraints);

  bool value = false;
  EXPECT_TRUE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_FALSE(source_->options()->video_leaky_bucket.Get(&value));
}

TEST_F(VideoSourceTest, InvalidOptionKeyOptional) {
  FakeConstraints constraints;
  constraints.AddOptional(
      MediaConstraintsInterface::kNoiseReduction, false);
  constraints.AddOptional("invalidKey", false);

  CreateVideoSource(&constraints);

  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
      kMaxWaitMs);
  bool value = true;
  EXPECT_TRUE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_FALSE(value);
}

TEST_F(VideoSourceTest, InvalidOptionKeyMandatory) {
  FakeConstraints constraints;
  constraints.AddMandatory(
      MediaConstraintsInterface::kNoiseReduction, false);
  constraints.AddMandatory("invalidKey", false);

  CreateVideoSource(&constraints);

  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
      kMaxWaitMs);
  bool value;
  EXPECT_FALSE(source_->options()->video_noise_reduction.Get(&value));
}

TEST_F(VideoSourceTest, InvalidOptionValueOptional) {
  FakeConstraints constraints;
  constraints.AddOptional(
      MediaConstraintsInterface::kNoiseReduction, "true");
  constraints.AddOptional(
      MediaConstraintsInterface::kLeakyBucket, "not boolean");

  CreateVideoSource(&constraints);

  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
      kMaxWaitMs);
  bool value = false;
  EXPECT_TRUE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_FALSE(source_->options()->video_leaky_bucket.Get(&value));
}

TEST_F(VideoSourceTest, InvalidOptionValueMandatory) {
  FakeConstraints constraints;
  // Optional constraints should be ignored if the mandatory constraints fail.
  constraints.AddOptional(
      MediaConstraintsInterface::kNoiseReduction, "false");
  // Values are case-sensitive and must be all lower-case.
  constraints.AddMandatory(
      MediaConstraintsInterface::kLeakyBucket, "True");

  CreateVideoSource(&constraints);

  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
      kMaxWaitMs);
  bool value;
  EXPECT_FALSE(source_->options()->video_noise_reduction.Get(&value));
}

TEST_F(VideoSourceTest, MixedOptionsAndConstraints) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMaxWidth, 352);
  constraints.AddMandatory(MediaConstraintsInterface::kMaxHeight, 288);
  constraints.AddOptional(MediaConstraintsInterface::kMaxFrameRate, 5);

  constraints.AddMandatory(
      MediaConstraintsInterface::kNoiseReduction, false);
  constraints.AddOptional(
      MediaConstraintsInterface::kNoiseReduction, true);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(352, format->width);
  EXPECT_EQ(288, format->height);
  EXPECT_EQ(30, format->framerate());

  bool value = true;
  EXPECT_TRUE(source_->options()->video_noise_reduction.Get(&value));
  EXPECT_FALSE(value);
  EXPECT_FALSE(source_->options()->video_leaky_bucket.Get(&value));
}

// Tests that the source starts video with the default resolution for
// screencast if no constraint is set.
TEST_F(VideoSourceTest, ScreencastResolutionNoConstraint) {
  capturer_->TestWithoutCameraFormats();
  capturer_->SetScreencast(true);

  CreateVideoSource();
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(640, format->width);
  EXPECT_EQ(480, format->height);
  EXPECT_EQ(30, format->framerate());
}

// Tests that the source starts video with the max width and height set by
// constraints for screencast.
TEST_F(VideoSourceTest, ScreencastResolutionWithConstraint) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMaxWidth, 480);
  constraints.AddMandatory(MediaConstraintsInterface::kMaxHeight, 270);

  capturer_->TestWithoutCameraFormats();
  capturer_->SetScreencast(true);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(480, format->width);
  EXPECT_EQ(270, format->height);
  EXPECT_EQ(30, format->framerate());
}

TEST_F(VideoSourceTest, MandatorySubOneFpsConstraints) {
  FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kMaxFrameRate, 0.5);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kEnded, state_observer_->state(),
                 kMaxWaitMs);
  ASSERT_TRUE(capturer_->GetCaptureFormat() == NULL);
}

TEST_F(VideoSourceTest, OptionalSubOneFpsConstraints) {
  FakeConstraints constraints;
  constraints.AddOptional(MediaConstraintsInterface::kMaxFrameRate, 0.5);

  CreateVideoSource(&constraints);
  EXPECT_EQ_WAIT(MediaSourceInterface::kLive, state_observer_->state(),
                 kMaxWaitMs);
  const cricket::VideoFormat* format = capturer_->GetCaptureFormat();
  ASSERT_TRUE(format != NULL);
  EXPECT_EQ(30, format->framerate());
}

