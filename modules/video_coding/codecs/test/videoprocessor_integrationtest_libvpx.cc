/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/ptr_util.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
// Codec settings.
const bool kResilienceOn = true;
const int kCifWidth = 352;
const int kCifHeight = 288;
#if !defined(WEBRTC_IOS)
const int kNumFramesShort = 100;
#endif
const int kNumFramesLong = 300;
const size_t kBitrateRdPerfKbps[] = {300, 600, 800, 1250, 1750, 2500};
}  // namespace

class VideoProcessorIntegrationTestLibvpx
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestLibvpx() {
    config_.filename = "foreman_cif";
    config_.filepath = ResourcePath(config_.filename, "yuv");
    config_.num_frames = kNumFramesLong;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;
    config_.hw_encoder = false;
    config_.hw_decoder = false;
    config_.encoded_frame_checker = &qp_frame_checker_;
  }

  void PrintRdPerf(std::map<size_t, std::vector<VideoStatistics>> rd_stats) {
    printf("--> Summary\n");
    printf("%11s %5s %6s %13s %13s %5s %7s %7s %7s %13s %13s\n", "uplink_kbps",
           "width", "height", "downlink_kbps", "framerate_fps", "psnr",
           "psnr_y", "psnr_u", "psnr_v", "enc_speed_fps", "dec_speed_fps");
    for (const auto& rd_stat : rd_stats) {
      const size_t bitrate_kbps = rd_stat.first;
      for (const auto& layer_stat : rd_stat.second) {
        printf(
            "%11zu %5zu %6zu %13zu %13.2f %5.2f %7.2f %7.2f %7.2f %13.2f "
            "%13.2f\n",
            bitrate_kbps, layer_stat.width, layer_stat.height,
            layer_stat.bitrate_kbps, layer_stat.framerate_fps,
            layer_stat.avg_psnr, layer_stat.avg_psnr_y, layer_stat.avg_psnr_u,
            layer_stat.avg_psnr_v, layer_stat.enc_speed_fps,
            layer_stat.dec_speed_fps);
      }
    }
  }

 private:
  // Verify that the QP parser returns the same QP as the encoder does.
  const class QpFrameChecker : public TestConfig::EncodedFrameChecker {
   public:
    void CheckEncodedFrame(webrtc::VideoCodecType codec,
                           const EncodedImage& encoded_frame) const override {
      int qp;
      if (codec == kVideoCodecVP8) {
        EXPECT_TRUE(
            vp8::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
      } else if (codec == kVideoCodecVP9) {
        EXPECT_TRUE(
            vp9::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
      } else {
        RTC_NOTREACHED();
      }
      EXPECT_EQ(encoded_frame.qp_, qp) << "Encoder QP != parsed bitstream QP.";
    }
  } qp_frame_checker_;
};

// Fails on iOS. See webrtc:4755.
#if !defined(WEBRTC_IOS)

#if !defined(RTC_DISABLE_VP9)
TEST_F(VideoProcessorIntegrationTestLibvpx, HighBitrateVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, 1, 1, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 36, 0.94, 0.92}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, ChangeBitrateVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, 1, 1, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {200, 30, 100},  // target_kbps, input_fps, frame_index_rate_update
      {700, 30, 200},
      {500, 30, kNumFramesLong}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.15, 0.5, 0.1, 0, 1},
      {15, 2, 0, 0.2, 0.5, 0.1, 0, 0},
      {10, 1, 0, 0.3, 0.5, 0.1, 0, 0}};

  std::vector<QualityThresholds> quality_thresholds = {
      {34, 33, 0.90, 0.88}, {38, 35, 0.95, 0.91}, {35, 34, 0.93, 0.90}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, ChangeFramerateVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, 1, 1, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {100, 24, 100},  // target_kbps, input_fps, frame_index_rate_update
      {100, 15, 200},
      {100, 10, kNumFramesLong}};

  // Framerate mismatch should be lower for lower framerate.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 2, 40, 0.4, 0.5, 0.2, 0, 1},
      {8, 2, 5, 0.2, 0.5, 0.2, 0, 0},
      {5, 2, 0, 0.2, 0.5, 0.3, 0, 0}};

  // Quality should be higher for lower framerates for the same content.
  std::vector<QualityThresholds> quality_thresholds = {
      {33, 32, 0.89, 0.87}, {33.5, 32, 0.90, 0.86}, {33.5, 31.5, 0.90, 0.85}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, DenoiserOnVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, 1, 1, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37.5, 36, 0.94, 0.93}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, VeryLowBitrateVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, 1, 1, false, true, true,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{50, 30, kNumFramesLong}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {15, 3, 75, 1.0, 0.5, 0.4, 1, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{28, 25, 0.80, 0.65}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// TODO(marpan): Add temporal layer test for VP9, once changes are in
// vp9 wrapper for this.

#endif  // !defined(RTC_DISABLE_VP9)

TEST_F(VideoProcessorIntegrationTestLibvpx, HighBitrateVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, 1, 1, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  // std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};
  // TODO(webrtc:8757): ARM VP8 encoder's quality is significantly worse
  // than quality of x86 version. Use lower thresholds for now.
  std::vector<QualityThresholds> quality_thresholds = {{35, 33, 0.91, 0.89}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

#endif  // !defined(WEBRTC_IOS)

// The tests below are currently disabled for Android. For ARM, the encoder
// uses |cpu_speed| = 12, as opposed to default |cpu_speed| <= 6 for x86,
// which leads to significantly different quality. The quality and rate control
// settings in the tests below are defined for encoder speed setting
// |cpu_speed| <= ~6. A number of settings would need to be significantly
// modified for the |cpu_speed| = 12 case. For now, keep the tests below
// disabled on Android. Some quality parameter in the above test has been
// adjusted to also pass for |cpu_speed| <= 12.

// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_ChangeBitrateVP8 DISABLED_ChangeBitrateVP8
#else
#define MAYBE_ChangeBitrateVP8 ChangeBitrateVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_ChangeBitrateVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, 1, 1, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {200, 30, 100},  // target_kbps, input_fps, frame_index_rate_update
      {800, 30, 200},
      {500, 30, kNumFramesLong}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1},
      {15, 1, 0, 0.1, 0.2, 0.1, 0, 0},
      {15, 1, 0, 0.3, 0.2, 0.1, 0, 0}};

  // std::vector<QualityThresholds> quality_thresholds = {
  //     {33, 32, 0.89, 0.88}, {38, 36, 0.94, 0.93}, {35, 34, 0.92, 0.91}};
  // TODO(webrtc:8757): ARM VP8 encoder's quality is significantly worse
  // than quality of x86 version. Use lower thresholds for now.
  std::vector<QualityThresholds> quality_thresholds = {
      {31.8, 31, 0.86, 0.85}, {36, 34.8, 0.92, 0.90}, {33.5, 32, 0.90, 0.88}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_ChangeFramerateVP8 DISABLED_ChangeFramerateVP8
#else
#define MAYBE_ChangeFramerateVP8 ChangeFramerateVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_ChangeFramerateVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, 1, 1, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {80, 24, 100},  // target_kbps, input_fps, frame_index_rate_update
      {80, 15, 200},
      {80, 10, kNumFramesLong}};

  // std::vector<RateControlThresholds> rc_thresholds = {
  //     {10, 2, 20, 0.4, 0.3, 0.1, 0, 1},
  //     {5, 2, 5, 0.3, 0.3, 0.1, 0, 0},
  //     {4, 2, 1, 0.2, 0.3, 0.2, 0, 0}};
  // TODO(webrtc:8757): ARM VP8 drops more frames than x86 version. Use lower
  // thresholds for now.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 2, 60, 0.5, 0.3, 0.3, 0, 1},
      {10, 2, 30, 0.3, 0.3, 0.3, 0, 0},
      {10, 2, 10, 0.2, 0.3, 0.2, 0, 0}};

  // std::vector<QualityThresholds> quality_thresholds = {
  //     {31, 30, 0.87, 0.86}, {32, 31, 0.89, 0.86}, {32, 30, 0.87, 0.82}};
  // TODO(webrtc:8757): ARM VP8 encoder's quality is significantly worse
  // than quality of x86 version. Use lower thresholds for now.
  std::vector<QualityThresholds> quality_thresholds = {
      {31, 30, 0.85, 0.84}, {31.5, 30.5, 0.86, 0.84}, {30.5, 29, 0.83, 0.78}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_TemporalLayersVP8 DISABLED_TemporalLayersVP8
#else
#define MAYBE_TemporalLayersVP8 TemporalLayersVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_TemporalLayersVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, 1, 3, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{200, 30, 150},
                                            {400, 30, kNumFramesLong}};

  // std::vector<RateControlThresholds> rc_thresholds = {
  //     {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}, {10, 2, 0, 0.1, 0.2, 0.1, 0, 1}};
  // TODO(webrtc:8757): ARM VP8 drops more frames than x86 version. Use lower
  // thresholds for now.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 2, 0.3, 0.2, 0.1, 0, 1}, {12, 2, 3, 0.1, 0.2, 0.1, 0, 1}};

  // Min SSIM drops because of high motion scene with complex backgound (trees).
  // std::vector<QualityThresholds> quality_thresholds = {{32, 30, 0.88, 0.85},
  //                                                     {33, 30, 0.89, 0.83}};
  // TODO(webrtc:8757): ARM VP8 encoder's quality is significantly worse
  // than quality of x86 version. Use lower thresholds for now.
  std::vector<QualityThresholds> quality_thresholds = {{31, 30, 0.85, 0.84},
                                                       {31, 28, 0.85, 0.75}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// Might be too slow on mobile platforms.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_MultiresVP8 DISABLED_MultiresVP8
#else
#define MAYBE_MultiresVP8 MultiresVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_MultiresVP8) {
  config_.filename = "ConferenceMotion_1280_720_50";
  config_.filepath = ResourcePath(config_.filename, "yuv");
  config_.num_frames = 100;
  config_.SetCodecSettings(kVideoCodecVP8, 3, 1, 3, true, true, false,
                           kResilienceOn, 1280, 720);

  std::vector<RateProfile> rate_profiles = {{1500, 30, config_.num_frames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 5, 0.2, 0.3, 0.1, 0, 1}};
  std::vector<QualityThresholds> quality_thresholds = {{34, 32, 0.90, 0.88}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// Might be too slow on mobile platforms.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_SimulcastVP8 DISABLED_SimulcastVP8
#else
#define MAYBE_SimulcastVP8 SimulcastVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_SimulcastVP8) {
  config_.filename = "ConferenceMotion_1280_720_50";
  config_.filepath = ResourcePath(config_.filename, "yuv");
  config_.num_frames = 100;
  config_.simulcast_adapted_encoder = true;
  config_.SetCodecSettings(kVideoCodecVP8, 3, 1, 3, true, true, false,
                           kResilienceOn, 1280, 720);

  std::vector<RateProfile> rate_profiles = {{1500, 30, config_.num_frames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {20, 5, 90, 0.8, 0.5, 0.3, 0, 1}};
  std::vector<QualityThresholds> quality_thresholds = {{34, 32, 0.90, 0.88}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

// Might be too slow on mobile platforms.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_SvcVP9 DISABLED_SvcVP9
#else
#define MAYBE_SvcVP9 SvcVP9
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx, MAYBE_SvcVP9) {
  config_.filename = "ConferenceMotion_1280_720_50";
  config_.filepath = ResourcePath(config_.filename, "yuv");
  config_.num_frames = 100;
  config_.SetCodecSettings(kVideoCodecVP9, 1, 3, 3, true, true, false,
                           kResilienceOn, 1280, 720);

  std::vector<RateProfile> rate_profiles = {{1500, 30, config_.num_frames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 5, 0.2, 0.3, 0.1, 0, 1}};
  std::vector<QualityThresholds> quality_thresholds = {{36, 34, 0.93, 0.91}};

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, DISABLED_MultiresVP8RdPerf) {
  config_.filename = "FourPeople_1280x720_30";
  config_.filepath = ResourcePath(config_.filename, "yuv");
  config_.num_frames = 300;
  config_.SetCodecSettings(kVideoCodecVP8, 3, 1, 3, true, true, false,
                           kResilienceOn, 1280, 720);

  std::map<size_t, std::vector<VideoStatistics>> rd_stats;
  for (size_t bitrate_kbps : kBitrateRdPerfKbps) {
    std::vector<RateProfile> rate_profiles = {
        {bitrate_kbps, 30, config_.num_frames}};

    ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr, nullptr,
                                nullptr);

    rd_stats[bitrate_kbps] =
        stats_.SliceAndCalcLayerVideoStatistic(0, config_.num_frames - 1);
  }

  PrintRdPerf(rd_stats);
}

TEST_F(VideoProcessorIntegrationTestLibvpx, DISABLED_SvcVP9RdPerf) {
  config_.filename = "FourPeople_1280x720_30";
  config_.filepath = ResourcePath(config_.filename, "yuv");
  config_.num_frames = 300;
  config_.SetCodecSettings(kVideoCodecVP9, 1, 3, 3, true, true, false,
                           kResilienceOn, 1280, 720);

  std::map<size_t, std::vector<VideoStatistics>> rd_stats;
  for (size_t bitrate_kbps : kBitrateRdPerfKbps) {
    std::vector<RateProfile> rate_profiles = {
        {bitrate_kbps, 30, config_.num_frames}};

    ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr, nullptr,
                                nullptr);

    rd_stats[bitrate_kbps] =
        stats_.SliceAndCalcLayerVideoStatistic(0, config_.num_frames - 1);
  }

  PrintRdPerf(rd_stats);
}

}  // namespace test
}  // namespace webrtc
