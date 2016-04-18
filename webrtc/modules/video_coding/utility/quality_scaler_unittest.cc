/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace {
static const int kNumSeconds = 10;
static const int kWidth = 1920;
static const int kHalfWidth = kWidth / 2;
static const int kHeight = 1080;
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kNormalQp = 30;
static const int kLowQpThreshold = 18;
static const int kHighQp = 40;
static const int kDisabledBadQpThreshold = 64;
static const int kLowInitialBitrateKbps = 300;
// These values need to be in sync with corresponding constants
// in quality_scaler.cc
static const int kMeasureSecondsFastUpscale = 2;
static const int kMeasureSecondsUpscale = 5;
static const int kMeasureSecondsDownscale = 5;
static const int kMinDownscaleDimension = 140;
}  // namespace

class QualityScalerTest : public ::testing::Test {
 public:
  // Temporal and spatial resolution.
  struct Resolution {
    int framerate;
    int width;
    int height;
  };

 protected:
  enum ScaleDirection {
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };
  enum BadQualityMetric { kDropFrame, kReportLowQP };

  QualityScalerTest() {
    input_frame_.CreateEmptyFrame(kWidth, kHeight, kWidth, kHalfWidth,
                                  kHalfWidth);
    qs_.Init(kLowQpThreshold, kHighQp, false, 0, 0, 0, kFramerate);
    qs_.OnEncodeFrame(input_frame_);
  }

  bool TriggerScale(ScaleDirection scale_direction) {
    qs_.OnEncodeFrame(input_frame_);
    int initial_width = qs_.GetScaledResolution().width;
    for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_.ReportQP(kLowQp);
          break;
        case kScaleDown:
          qs_.ReportDroppedFrame();
          break;
        case kKeepScaleAtHighQp:
          qs_.ReportQP(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_.ReportQP(kHighQp + 1);
          break;
      }
      qs_.OnEncodeFrame(input_frame_);
      if (qs_.GetScaledResolution().width != initial_width)
        return true;
    }

    return false;
  }

  void ExpectOriginalFrame() {
    EXPECT_EQ(&input_frame_, &qs_.GetScaledFrame(input_frame_))
        << "Using scaled frame instead of original input.";
  }

  void ExpectScaleUsingReportedResolution() {
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    const VideoFrame& scaled_frame = qs_.GetScaledFrame(input_frame_);
    EXPECT_EQ(res.width, scaled_frame.width());
    EXPECT_EQ(res.height, scaled_frame.height());
  }

  void ContinuouslyDownscalesByHalfDimensionsAndBackUp();

  void DoesNotDownscaleFrameDimensions(int width, int height);

  Resolution TriggerResolutionChange(BadQualityMetric dropframe_lowqp,
                                     int num_second,
                                     int initial_framerate);

  void VerifyQualityAdaptation(int initial_framerate,
                               int seconds_downscale,
                               int seconds_upscale,
                               bool expect_spatial_resize,
                               bool expect_framerate_reduction);

  void DownscaleEndsAt(int input_width,
                       int input_height,
                       int end_width,
                       int end_height);

  QualityScaler qs_;
  VideoFrame input_frame_;
};

TEST_F(QualityScalerTest, UsesOriginalFrameInitially) {
  ExpectOriginalFrame();
}

TEST_F(QualityScalerTest, ReportsOriginalResolutionInitially) {
  qs_.OnEncodeFrame(input_frame_);
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(input_frame_.width(), res.width);
  EXPECT_EQ(input_frame_.height(), res.height);
}

TEST_F(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within " << kNumSeconds
                                        << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_.width());
  EXPECT_LT(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, KeepsScaleAtHighQp) {
  EXPECT_FALSE(TriggerScale(kKeepScaleAtHighQp))
      << "Downscale at high threshold which should keep scale.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(res.width, input_frame_.width());
  EXPECT_EQ(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, DownscalesAboveHighQp) {
  EXPECT_TRUE(TriggerScale(kScaleDownAboveHighQp))
      << "No downscale within " << kNumSeconds << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_.width());
  EXPECT_LT(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 3; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.ReportDroppedFrame();
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    if (qs_.GetScaledResolution().width < input_frame_.width())
      return;
  }

  FAIL() << "No downscale within " << kNumSeconds << " seconds.";
}

TEST_F(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 2; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";

    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";
  }
}

void QualityScalerTest::ContinuouslyDownscalesByHalfDimensionsAndBackUp() {
  const int initial_min_dimension = input_frame_.width() < input_frame_.height()
                                        ? input_frame_.width()
                                        : input_frame_.height();
  int min_dimension = initial_min_dimension;
  int current_shift = 0;
  // Drop all frames to force-trigger downscaling.
  while (min_dimension >= 2 * kMinDownscaleDimension) {
    EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within "
                                          << kNumSeconds << " seconds.";
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    ++current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Make sure we can scale back with good-quality frames.
  while (min_dimension < initial_min_dimension) {
    EXPECT_TRUE(TriggerScale(kScaleUp)) << "No upscale within " << kNumSeconds
                                        << " seconds.";
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    --current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Verify we don't start upscaling after further low use.
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kLowQp);
    ExpectOriginalFrame();
  }
}

TEST_F(QualityScalerTest, ContinuouslyDownscalesByHalfDimensionsAndBackUp) {
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

TEST_F(QualityScalerTest,
       ContinuouslyDownscalesOddResolutionsByHalfDimensionsAndBackUp) {
  const int kOddWidth = 517;
  const int kHalfOddWidth = (kOddWidth + 1) / 2;
  const int kOddHeight = 1239;
  input_frame_.CreateEmptyFrame(kOddWidth, kOddHeight, kOddWidth, kHalfOddWidth,
                                kHalfOddWidth);
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

void QualityScalerTest::DoesNotDownscaleFrameDimensions(int width, int height) {
  input_frame_.CreateEmptyFrame(width, height, width, (width + 1) / 2,
                                (width + 1) / 2);

  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale of minimal-size frame.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxWidth) {
  DoesNotDownscaleFrameDimensions(1, kHeight);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxHeight) {
  DoesNotDownscaleFrameDimensions(kWidth, 1);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1Px) {
  DoesNotDownscaleFrameDimensions(1, 1);
}

QualityScalerTest::Resolution QualityScalerTest::TriggerResolutionChange(
    BadQualityMetric dropframe_lowqp,
    int num_second,
    int initial_framerate) {
  QualityScalerTest::Resolution res;
  res.framerate = initial_framerate;
  qs_.OnEncodeFrame(input_frame_);
  res.width = qs_.GetScaledResolution().width;
  res.height = qs_.GetScaledResolution().height;
  for (int i = 0; i < kFramerate * num_second; ++i) {
    switch (dropframe_lowqp) {
      case kReportLowQP:
        qs_.ReportQP(kLowQp);
        break;
      case kDropFrame:
        qs_.ReportDroppedFrame();
        break;
    }
    qs_.OnEncodeFrame(input_frame_);
    // Simulate the case when SetRates is called right after reducing
    // framerate.
    qs_.ReportFramerate(initial_framerate);
    res.framerate = qs_.GetTargetFramerate();
    if (res.framerate != -1)
      qs_.ReportFramerate(res.framerate);
    res.width = qs_.GetScaledResolution().width;
    res.height = qs_.GetScaledResolution().height;
  }
  return res;
}

void QualityScalerTest::VerifyQualityAdaptation(
    int initial_framerate,
    int seconds_downscale,
    int seconds_upscale,
    bool expect_spatial_resize,
    bool expect_framerate_reduction) {
  qs_.Init(kLowQpThreshold, kDisabledBadQpThreshold, true, 0, 0, 0,
           initial_framerate);
  qs_.OnEncodeFrame(input_frame_);
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;

  // Test reducing framerate by dropping frame continuously.
  QualityScalerTest::Resolution res =
      TriggerResolutionChange(kDropFrame, seconds_downscale, initial_framerate);

  if (expect_framerate_reduction) {
    EXPECT_LT(res.framerate, initial_framerate);
  } else {
    // No framerate reduction, video decimator should be disabled.
    EXPECT_EQ(-1, res.framerate);
  }

  if (expect_spatial_resize) {
    EXPECT_LT(res.width, init_width);
    EXPECT_LT(res.height, init_height);
  } else {
    EXPECT_EQ(init_width, res.width);
    EXPECT_EQ(init_height, res.height);
  }

  // The "seconds * 1.5" is to ensure spatial resolution to recover.
  // For example, in 6 seconds test, framerate reduction happens in the first
  // 3 seconds from 30fps to 15fps and causes the buffer size to be half of the
  // original one. Then it will take only 45 samples to downscale (twice in 90
  // samples). So to recover the resolution changes, we need more than 10
  // seconds (i.e, seconds_upscale * 1.5). This is because the framerate
  // increases before spatial size recovers, so it will take 150 samples to
  // recover spatial size (300 for twice).
  res = TriggerResolutionChange(kReportLowQP, seconds_upscale * 1.5,
                                initial_framerate);
  EXPECT_EQ(-1, res.framerate);
  EXPECT_EQ(init_width, res.width);
  EXPECT_EQ(init_height, res.height);
}

// In 3 seconds test, only framerate adjusting should happen and 5 second
// upscaling duration, only a framerate adjusting should happen.
TEST_F(QualityScalerTest, ChangeFramerateOnly) {
  VerifyQualityAdaptation(kFramerate, kMeasureSecondsDownscale,
                          kMeasureSecondsUpscale, false, true);
}

// In 6 seconds test, framerate adjusting and scaling are both
// triggered, it shows that scaling would happen after framerate
// adjusting.
TEST_F(QualityScalerTest, ChangeFramerateAndSpatialSize) {
  VerifyQualityAdaptation(kFramerate, kMeasureSecondsDownscale * 2,
                          kMeasureSecondsUpscale * 2, true, true);
}

// When starting from a low framerate, only spatial size will be changed.
TEST_F(QualityScalerTest, ChangeSpatialSizeOnly) {
  qs_.ReportFramerate(kFramerate >> 1);
  VerifyQualityAdaptation(kFramerate >> 1, kMeasureSecondsDownscale * 2,
                          kMeasureSecondsUpscale * 2, true, false);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsWidth) {
  DoesNotDownscaleFrameDimensions(
      2 * kMinDownscaleDimension - 1, 1000);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsHeight) {
  DoesNotDownscaleFrameDimensions(
      1000, 2 * kMinDownscaleDimension - 1);
}

TEST_F(QualityScalerTest, DownscaleToVgaOnLowInitialBitrate) {
  static const int kWidth720p = 1280;
  static const int kHeight720p = 720;
  static const int kInitialBitrateKbps = 300;
  input_frame_.CreateEmptyFrame(kWidth720p, kHeight720p, kWidth720p,
                                kWidth720p / 2, kWidth720p / 2);
  qs_.Init(kLowQpThreshold, kDisabledBadQpThreshold, true, kInitialBitrateKbps,
           kWidth720p, kHeight720p, kFramerate);
  qs_.OnEncodeFrame(input_frame_);
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;
  EXPECT_EQ(640, init_width);
  EXPECT_EQ(360, init_height);
}

TEST_F(QualityScalerTest, DownscaleToQvgaOnLowerInitialBitrate) {
  static const int kWidth720p = 1280;
  static const int kHeight720p = 720;
  static const int kInitialBitrateKbps = 200;
  input_frame_.CreateEmptyFrame(kWidth720p, kHeight720p, kWidth720p,
                                kWidth720p / 2, kWidth720p / 2);
  qs_.Init(kLowQpThreshold, kDisabledBadQpThreshold, true, kInitialBitrateKbps,
           kWidth720p, kHeight720p, kFramerate);
  qs_.OnEncodeFrame(input_frame_);
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;
  EXPECT_EQ(320, init_width);
  EXPECT_EQ(180, init_height);
}

TEST_F(QualityScalerTest, DownscaleAfterMeasuredSecondsThenSlowerBackUp) {
  QualityScalerTest::Resolution initial_res;
  qs_.Init(kLowQpThreshold, kHighQp, false, 0, kWidth, kHeight, kFramerate);
  qs_.OnEncodeFrame(input_frame_);
  initial_res.width = qs_.GetScaledResolution().width;
  initial_res.height = qs_.GetScaledResolution().height;

  // Should not downscale if less than kMeasureSecondsDownscale seconds passed.
  for (int i = 0; i < kFramerate * kMeasureSecondsDownscale - 1; ++i) {
    qs_.ReportQP(kHighQp + 1);
    qs_.OnEncodeFrame(input_frame_);
  }
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);

  // Should downscale if more than kMeasureSecondsDownscale seconds passed (add
  // last frame).
  qs_.ReportQP(kHighQp + 1);
  qs_.OnEncodeFrame(input_frame_);
  EXPECT_GT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_GT(initial_res.height, qs_.GetScaledResolution().height);

  // Should not upscale if less than kMeasureSecondsUpscale seconds passed since
  // we saw issues initially (have already gone down).
  for (int i = 0; i < kFramerate * kMeasureSecondsUpscale - 1; ++i) {
    qs_.ReportQP(kLowQp);
    qs_.OnEncodeFrame(input_frame_);
  }
  EXPECT_GT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_GT(initial_res.height, qs_.GetScaledResolution().height);

  // Should upscale (back to initial) if kMeasureSecondsUpscale seconds passed
  // (add last frame).
  qs_.ReportQP(kLowQp);
  qs_.OnEncodeFrame(input_frame_);
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);
}

TEST_F(QualityScalerTest, UpscaleQuicklyInitiallyAfterMeasuredSeconds) {
  QualityScalerTest::Resolution initial_res;
  qs_.Init(kLowQpThreshold, kHighQp, false, kLowInitialBitrateKbps, kWidth,
           kHeight, kFramerate);
  qs_.OnEncodeFrame(input_frame_);
  initial_res.width = qs_.GetScaledResolution().width;
  initial_res.height = qs_.GetScaledResolution().height;

  // Should not upscale if less than kMeasureSecondsFastUpscale seconds passed.
  for (int i = 0; i < kFramerate * kMeasureSecondsFastUpscale - 1; ++i) {
    qs_.ReportQP(kLowQp);
    qs_.OnEncodeFrame(input_frame_);
  }
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);

  // Should upscale if kMeasureSecondsFastUpscale seconds passed (add last
  // frame).
  qs_.ReportQP(kLowQp);
  qs_.OnEncodeFrame(input_frame_);
  EXPECT_LT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_LT(initial_res.height, qs_.GetScaledResolution().height);
}

void QualityScalerTest::DownscaleEndsAt(int input_width,
                                        int input_height,
                                        int end_width,
                                        int end_height) {
  // Create a frame with 2x expected end width/height to verify that we can
  // scale down to expected end width/height.
  input_frame_.CreateEmptyFrame(input_width, input_height, input_width,
                                (input_width + 1) / 2, (input_width + 1) / 2);

  int last_width = input_width;
  int last_height = input_height;
  // Drop all frames to force-trigger downscaling.
  while (true) {
    TriggerScale(kScaleDown);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    if (last_width == res.width) {
      EXPECT_EQ(last_height, res.height);
      EXPECT_EQ(end_width, res.width);
      EXPECT_EQ(end_height, res.height);
      break;
    }
    last_width = res.width;
    last_height = res.height;
  }
}

TEST_F(QualityScalerTest, DownscalesTo320x180) {
  DownscaleEndsAt(640, 360, 320, 180);
}

TEST_F(QualityScalerTest, DownscalesTo180x320) {
  DownscaleEndsAt(360, 640, 180, 320);
}

TEST_F(QualityScalerTest, DownscalesFrom1280x720To320x180) {
  DownscaleEndsAt(1280, 720, 320, 180);
}

TEST_F(QualityScalerTest, DoesntDownscaleInitialQvga) {
  DownscaleEndsAt(320, 180, 320, 180);
}

}  // namespace webrtc
