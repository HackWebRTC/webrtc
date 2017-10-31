/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/videoencodersoftwarefallbackwrapper.h"

#include <utility>

#include "api/video/i420_buffer.h"
#include "modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const int kWidth = 320;
const int kHeight = 240;
const int kNumCores = 2;
const uint32_t kFramerate = 30;
const size_t kMaxPayloadSize = 800;
const int kDefaultMinPixelsPerFrame = 320 * 180;
}  // namespace

class VideoEncoderSoftwareFallbackWrapperTest : public ::testing::Test {
 protected:
  VideoEncoderSoftwareFallbackWrapperTest()
      : VideoEncoderSoftwareFallbackWrapperTest("") {}
  explicit VideoEncoderSoftwareFallbackWrapperTest(
      const std::string& field_trials)
      : override_field_trials_(field_trials),
        fake_encoder_(new CountingFakeEncoder()),
        fallback_wrapper_(cricket::VideoCodec("VP8"),
                          std::unique_ptr<VideoEncoder>(fake_encoder_)) {}

  class CountingFakeEncoder : public VideoEncoder {
   public:
    int32_t InitEncode(const VideoCodec* codec_settings,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      ++init_encode_count_;
      return init_encode_return_code_;
    }
    int32_t Encode(const VideoFrame& frame,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      ++encode_count_;
      if (encode_complete_callback_ &&
          encode_return_code_ == WEBRTC_VIDEO_CODEC_OK) {
        CodecSpecificInfo info;
        info.codec_name = ImplementationName();
        encode_complete_callback_->OnEncodedImage(EncodedImage(), &info,
                                                  nullptr);
      }
      return encode_return_code_;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      encode_complete_callback_ = callback;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() override {
      ++release_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {
      ++set_channel_parameters_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t SetRateAllocation(const BitrateAllocation& bitrate_allocation,
                              uint32_t framerate) override {
      ++set_rates_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    bool SupportsNativeHandle() const override {
      ++supports_native_handle_count_;
      return supports_native_handle_;
    }

    const char* ImplementationName() const override {
      return "fake-encoder";
    }

    VideoEncoder::ScalingSettings GetScalingSettings() const override {
      return VideoEncoder::ScalingSettings(true);
    }

    int init_encode_count_ = 0;
    int32_t init_encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int32_t encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int encode_count_ = 0;
    EncodedImageCallback* encode_complete_callback_ = nullptr;
    int release_count_ = 0;
    int set_channel_parameters_count_ = 0;
    int set_rates_count_ = 0;
    mutable int supports_native_handle_count_ = 0;
    bool supports_native_handle_ = false;
  };

  class FakeEncodedImageCallback : public EncodedImageCallback {
   public:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* fragmentation) override {
      ++callback_count_;
      last_codec_name_ = codec_specific_info->codec_name;
      return Result(Result::OK, callback_count_);
    }
    int callback_count_ = 0;
    std::string last_codec_name_;
  };

  void UtilizeFallbackEncoder();
  void FallbackFromEncodeRequest();
  void EncodeFrame();
  void EncodeFrame(int expected_ret);
  void CheckLastEncoderName(const char* expected_name) {
    EXPECT_STREQ(expected_name, callback_.last_codec_name_.c_str());
  }

  test::ScopedFieldTrials override_field_trials_;
  FakeEncodedImageCallback callback_;
  // |fake_encoder_| is owned and released by |fallback_wrapper_|.
  CountingFakeEncoder* fake_encoder_;
  VideoEncoderSoftwareFallbackWrapper fallback_wrapper_;
  VideoCodec codec_ = {};
  std::unique_ptr<VideoFrame> frame_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

void VideoEncoderSoftwareFallbackWrapperTest::EncodeFrame() {
  EncodeFrame(WEBRTC_VIDEO_CODEC_OK);
}

void VideoEncoderSoftwareFallbackWrapperTest::EncodeFrame(int expected_ret) {
  rtc::scoped_refptr<I420Buffer> buffer =
      I420Buffer::Create(codec_.width, codec_.height);
  I420Buffer::SetBlack(buffer);
  std::vector<FrameType> types(1, kVideoFrameKey);

  frame_.reset(new VideoFrame(buffer, 0, 0, webrtc::kVideoRotation_0));
  EXPECT_EQ(expected_ret, fallback_wrapper_.Encode(*frame_, nullptr, &types));
}

void VideoEncoderSoftwareFallbackWrapperTest::UtilizeFallbackEncoder() {
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  EXPECT_EQ(&callback_, fake_encoder_->encode_complete_callback_);

  // Register with failing fake encoder. Should succeed with VP8 fallback.
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = kFramerate;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  std::unique_ptr<TemporalLayersFactory> tl_factory(
      new TemporalLayersFactory());
  codec_.VP8()->tl_factory = tl_factory.get();
  rate_allocator_.reset(
      new SimulcastRateAllocator(codec_, std::move(tl_factory)));

  fake_encoder_->init_encode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      fallback_wrapper_.SetRateAllocation(
          rate_allocator_->GetAllocation(300000, kFramerate), kFramerate));

  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

void VideoEncoderSoftwareFallbackWrapperTest::FallbackFromEncodeRequest() {
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = kFramerate;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  std::unique_ptr<TemporalLayersFactory> tl_factory(
      new TemporalLayersFactory());
  codec_.VP8()->tl_factory = tl_factory.get();
  rate_allocator_.reset(
      new SimulcastRateAllocator(codec_, std::move(tl_factory)));
  fallback_wrapper_.InitEncode(&codec_, 2, kMaxPayloadSize);
  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      fallback_wrapper_.SetRateAllocation(
          rate_allocator_->GetAllocation(300000, kFramerate), kFramerate));
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);

  // Have the non-fallback encoder request a software fallback.
  fake_encoder_->encode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  // Single encode request, which returned failure.
  EXPECT_EQ(encode_count + 1, fake_encoder_->encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, InitializesEncoder) {
  VideoCodec codec = {};
  fallback_wrapper_.InitEncode(&codec, 2, kMaxPayloadSize);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, EncodeRequestsFallback) {
  FallbackFromEncodeRequest();
  // After fallback, further encodes shouldn't hit the fake encoder.
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, CanUtilizeFallbackEncoder) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderReleasedDuringFallback) {
  EXPECT_EQ(0, fake_encoder_->release_count_);
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_->release_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
  // No extra release when the fallback is released.
  EXPECT_EQ(1, fake_encoder_->release_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderNotEncodingDuringFallback) {
  UtilizeFallbackEncoder();
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       CanRegisterCallbackWhileUsingFallbackEncoder) {
  UtilizeFallbackEncoder();
  // Registering an encode-complete callback should still work when fallback
  // encoder is being used.
  FakeEncodedImageCallback callback2;
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback2);
  EXPECT_EQ(&callback2, fake_encoder_->encode_complete_callback_);

  // Encoding a frame using the fallback should arrive at the new callback.
  std::vector<FrameType> types(1, kVideoFrameKey);
  frame_->set_timestamp(frame_->timestamp() + 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.Encode(*frame_, nullptr, &types));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SetChannelParametersForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(0, fake_encoder_->set_channel_parameters_count_);
  fallback_wrapper_.SetChannelParameters(1, 1);
  EXPECT_EQ(1, fake_encoder_->set_channel_parameters_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SetRatesForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_->set_rates_count_);
  fallback_wrapper_.SetRateAllocation(BitrateAllocation(), 1);
  EXPECT_EQ(2, fake_encoder_->set_rates_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleForwardedWithoutFallback) {
  fallback_wrapper_.SupportsNativeHandle();
  EXPECT_EQ(1, fake_encoder_->supports_native_handle_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleNotForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  fallback_wrapper_.SupportsNativeHandle();
  EXPECT_EQ(0, fake_encoder_->supports_native_handle_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, ReportsImplementationName) {
  codec_.width = kWidth;
  codec_.height = kHeight;
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize);
  EncodeFrame();
  CheckLastEncoderName("fake-encoder");
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       ReportsFallbackImplementationName) {
  UtilizeFallbackEncoder();
  // Hard coded expected value since libvpx is the software implementation name
  // for VP8. Change accordingly if the underlying implementation does.
  CheckLastEncoderName("libvpx");
}

namespace {
const int kLowKbps = 220;
const int kHighKbps = 300;
const int kMinLowDurationMs = 4000;
const int kMinPixelsPerFrame = 1;
const int kMinPixelsStop = 320 * 180;
const char kFieldTrial[] = "WebRTC-VP8-Forced-Fallback-Encoder";
}  // namespace

class ForcedFallbackTest : public VideoEncoderSoftwareFallbackWrapperTest {
 public:
  explicit ForcedFallbackTest(const std::string& field_trials)
      : VideoEncoderSoftwareFallbackWrapperTest(field_trials) {}

  ~ForcedFallbackTest() override {}

 protected:
  void SetUp() override {
    clock_.SetTimeMicros(1234);
    ConfigureVp8Codec();
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.InitEncode(
                                         &codec_, kNumCores, kMaxPayloadSize));
    EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  }

  void TearDown() override {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
  }

  void ConfigureVp8Codec() {
    fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
    std::unique_ptr<TemporalLayersFactory> tl_factory(
        new TemporalLayersFactory());
    codec_.codecType = kVideoCodecVP8;
    codec_.maxFramerate = kFramerate;
    codec_.width = kWidth;
    codec_.height = kHeight;
    codec_.VP8()->numberOfTemporalLayers = 1;
    codec_.VP8()->automaticResizeOn = true;
    codec_.VP8()->frameDroppingOn = true;
    codec_.VP8()->tl_factory = tl_factory.get();
    rate_allocator_.reset(
        new SimulcastRateAllocator(codec_, std::move(tl_factory)));
  }

  void SetRateAllocation(uint32_t bitrate_kbps) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.SetRateAllocation(
                                         rate_allocator_->GetAllocation(
                                             bitrate_kbps * 1000, kFramerate),
                                         kFramerate));
  }

  void EncodeFrameAndVerifyLastName(const char* expected_name) {
    EncodeFrameAndVerifyLastName(expected_name, WEBRTC_VIDEO_CODEC_OK);
  }

  void EncodeFrameAndVerifyLastName(const char* expected_name,
                                    int expected_ret) {
    EncodeFrame(expected_ret);
    CheckLastEncoderName(expected_name);
  }

  rtc::ScopedFakeClock clock_;
};

class ForcedFallbackTestEnabled : public ForcedFallbackTest {
 public:
  ForcedFallbackTestEnabled()
      : ForcedFallbackTest(std::string(kFieldTrial) + "/Enabled-" +
                           std::to_string(kLowKbps) + "," +
                           std::to_string(kHighKbps) + "," +
                           std::to_string(kMinLowDurationMs) + "," +
                           std::to_string(kMinPixelsPerFrame) + "/") {}
};

class ForcedFallbackTestDisabled : public ForcedFallbackTest {
 public:
  ForcedFallbackTestDisabled()
      : ForcedFallbackTest(std::string(kFieldTrial) + "/Disabled/") {}
};

TEST_F(ForcedFallbackTestDisabled, NoFallbackWithoutFieldTrial) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIfAtLowLimit) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, NoFallbackIfNotAtLowLimit) {
  // Bitrate above low threshold.
  SetRateAllocation(kLowKbps + 1);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, NoFallbackIfResolutionIsTooLarge) {
  // Resolution above max pixels.
  codec_.width += 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIfMinDurationPassed) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration not passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs - 1));
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1));
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, FallbackStartTimeResetIfAboveLowLimit) {
  // Bitrate at low threshold, start time set.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration not passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs - 1));
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Bitrate above low threshold, start time reset.
  SetRateAllocation(kLowKbps + 1);
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1));
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Bitrate at low threshold, start time set.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration not passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs - 1));
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1));
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, FallbackEndsIfAtHighLimit) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");
  // Bitrate below high threshold, expect fallback.
  SetRateAllocation(kHighKbps - 1);
  EncodeFrameAndVerifyLastName("libvpx");
  // Bitrate at high threshold, expect fallback ended.
  SetRateAllocation(kHighKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, MultipleStartEndFallback) {
  const int kNumRuns = 5;
  for (int i = 0; i < kNumRuns; ++i) {
    // Bitrate at low threshold.
    SetRateAllocation(kLowKbps);
    EncodeFrameAndVerifyLastName("fake-encoder");
    // Duration passed, expect fallback.
    clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
    EncodeFrameAndVerifyLastName("libvpx");
    // Bitrate at high threshold, expect fallback ended.
    SetRateAllocation(kHighKbps);
    EncodeFrameAndVerifyLastName("fake-encoder");
  }
}

TEST_F(ForcedFallbackTestEnabled, DropsFirstNonNativeFrameAfterFallbackEnds) {
  fake_encoder_->supports_native_handle_ = true;

  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");
  // Bitrate at high threshold, fallback should be ended but first non-native
  // frame dropped (i.e. frame not encoded).
  SetRateAllocation(kHighKbps);
  EncodeFrameAndVerifyLastName("libvpx", WEBRTC_VIDEO_CODEC_ERROR);
  // Next frame should be encoded.
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsKeptWhenInitEncodeIsCalled) {
  // Bitrate below low threshold.
  SetRateAllocation(kLowKbps - 1);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder, still expect fallback.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);  // No change.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsEndedWhenResolutionIsTooLarge) {
  // Bitrate below low threshold.
  SetRateAllocation(kLowKbps - 1);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with a larger resolution, expect no fallback.
  codec_.width += 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(2, fake_encoder_->init_encode_count_);
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsEndedForNonValidSettings) {
  // Bitrate below low threshold.
  SetRateAllocation(kLowKbps - 1);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with invalid setting, expect no fallback.
  codec_.VP8()->numberOfTemporalLayers = 2;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(2, fake_encoder_->init_encode_count_);
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Re-initialize encoder with valid setting but fallback disabled from now on.
  codec_.VP8()->numberOfTemporalLayers = 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(3, fake_encoder_->init_encode_count_);
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect no fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, GetScaleSettingsWithoutFallback) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Default min pixels per frame should be used.
  const auto settings = fallback_wrapper_.GetScalingSettings();
  EXPECT_TRUE(settings.enabled);
  EXPECT_EQ(kDefaultMinPixelsPerFrame, settings.min_pixels_per_frame);
}

TEST_F(ForcedFallbackTestEnabled, GetScaleSettingsWithFallback) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");
  // Configured min pixels per frame should be used.
  const auto settings = fallback_wrapper_.GetScalingSettings();
  EXPECT_TRUE(settings.enabled);
  EXPECT_EQ(kMinPixelsPerFrame, settings.min_pixels_per_frame);
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsKeptIfResolutionIsTooSmall) {
  // Bitrate at low threshold.
  SetRateAllocation(kLowKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
  // Duration passed, expect fallback.
  clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(kMinLowDurationMs));
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with a resolution less than |kMinPixelsStop|.
  codec_.height = kMinPixelsStop / codec_.width - 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);  // No change
  SetRateAllocation(kHighKbps - 1);
  EncodeFrameAndVerifyLastName("libvpx");
  // Bitrate at high threshold but resolution too small for fallback to end.
  SetRateAllocation(kHighKbps);
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with a resolution equal to |kMinPixelsStop|.
  codec_.height++;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);  // No change
  SetRateAllocation(kHighKbps - 1);
  EncodeFrameAndVerifyLastName("libvpx");
  // Bitrate at high threshold and resolution large enough for fallback to end.
  SetRateAllocation(kHighKbps);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

}  // namespace webrtc
