/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests the QmResolution class
 * In particular, for the selection of spatial and/or temporal down-sampling.
 */

#include <gtest/gtest.h>

#include "modules/video_coding/main/source/qm_select.h"
#include "modules/interface/module_common_types.h"

namespace webrtc {

class QmSelectTest : public ::testing::Test {
 protected:
  QmSelectTest()
      :  qm_resolution_(new VCMQmResolution()),
         content_metrics_(new VideoContentMetrics()),
         qm_scale_(NULL) {
  }
  VCMQmResolution* qm_resolution_;
  VideoContentMetrics* content_metrics_;
  VCMResolutionScale* qm_scale_;

  void InitQmNativeData(float initial_bit_rate, int user_frame_rate,
                        int native_width, int native_height);

  void UpdateQmEncodedFrame(int* encoded_size, int num_updates);

  void UpdateQmRateData(int* target_rate,
                        int* encoder_sent_rate,
                        int* incoming_frame_rate,
                        uint8_t* fraction_lost,
                        int num_updates);

  void UpdateQmContentData(float motion_metric,
                           float spatial_metric,
                           float spatial_metric_horiz,
                           float spatial_metric_vert);

  bool IsSelectedActionCorrect(VCMResolutionScale* qm_scale,
                               uint8_t fac_width,
                               uint8_t fac_height,
                               uint8_t fac_temp);

  void TearDown() {
    delete qm_resolution_;
    delete content_metrics_;
  }
};

TEST_F(QmSelectTest, HandleInputs) {
  // Expect parameter error. Initialize with invalid inputs.
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 0, 640, 480));
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 30, 640, 0));
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 30, 0, 480));

  // Expect uninitialized error.: No valid initialization before selection.
  EXPECT_EQ(-7, qm_resolution_->SelectResolution(&qm_scale_));

  VideoContentMetrics* content_metrics = NULL;
  EXPECT_EQ(0, qm_resolution_->Initialize(1000, 30, 640, 480));
  qm_resolution_->UpdateContent(content_metrics);
  // Content metrics are NULL: Expect success and no down-sampling action.
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// No down-sampling action at high rates.
TEST_F(QmSelectTest, NoActionHighRate) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(800, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {800, 800, 800};
  int encoder_sent_rate[] = {800, 800, 800};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  UpdateQmContentData(0.01f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(0, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// Rate is well below transition, down-sampling action is taken,
// depending on the content state.
TEST_F(QmSelectTest, DownActionLowRate) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial: 2x2 spatial expected.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, low spatial: no action expected: content is too low.
  UpdateQmContentData(0.01f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(0, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, low spatial: 2x2 spatial expected.
  UpdateQmContentData(0.06f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  qm_resolution_->ResetDownSamplingState();
  // High motion, high spatial: 1/2 temporal expected.
  UpdateQmContentData(0.1f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(4, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial: 1/2 temporal expected.
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, high spatial: 1/2 temporal expected.
  UpdateQmContentData(0.06f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(7, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  qm_resolution_->ResetDownSamplingState();
  // High motion, medium spatial: 2x2 spatial expected.
  UpdateQmContentData(0.1f, 0.03f, 0.03f, 0.03f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(5, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, medium spatial: high frame rate, so 1/2 temporal expected.
  UpdateQmContentData(0.01f, 0.03f, 0.03f, 0.03f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(2, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, medium spatial: high frame rate, so 1/2 temporal expected.
  UpdateQmContentData(0.06f, 0.03f, 0.03f, 0.03f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(8, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));
}

// Rate mis-match is high, and we have over-shooting.
// since target rate is below max for down-sampling, down-sampling is selected.
TEST_F(QmSelectTest, DownActionHighRateMMOvershoot) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(450, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {450, 450, 450};
  int encoder_sent_rate[] = {900, 900, 900};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));
}

// Rate mis-match is high, target rate is below max for down-sampling,
// but since we have consistent under-shooting, no down-sampling action.
TEST_F(QmSelectTest, NoActionHighRateMMUndershoot) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(450, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {450, 450, 450};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// Buffer is underflowing, and target rate is below max for down-sampling,
// so action is taken.
TEST_F(QmSelectTest, DownActionBufferUnderflow) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(450, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update with encoded size over a number of frames.
  // per-frame bandwidth = 15 = 450/30: simulate (decoder) buffer underflow:
  int encoded_size[] = {200, 100, 50, 30, 60, 40, 20, 30, 20, 40};
  UpdateQmEncodedFrame(encoded_size, 10);

  // Update rates for a sequence of intervals.
  int target_rate[] = {450, 450, 450};
  int encoder_sent_rate[] = {450, 450, 450};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));
}

// Target rate is below max for down-sampling, but buffer level is stable,
// so no action is taken.
TEST_F(QmSelectTest, NoActionBufferStable) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(450, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update with encoded size over a number of frames.
  // per-frame bandwidth = 15 = 450/30: simulate stable (decoder) buffer levels.
  int32_t encoded_size[] = {40, 10, 10, 16, 18, 20, 17, 20, 16, 15};
  UpdateQmEncodedFrame(encoded_size, 10);

  // Update rates for a sequence of intervals.
  int target_rate[] = {450, 450, 450};
  int encoder_sent_rate[] = {450, 450, 450};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// Very low rate, but no spatial down-sampling below some size (QCIF).
TEST_F(QmSelectTest, LimitDownSpatialAction) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(10, 30, 176, 144);

  // Update with encoder frame size.
  uint16_t codec_width = 176;
  uint16_t codec_height = 144;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(0, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {10, 10, 10};
  int encoder_sent_rate[] = {10, 10, 10};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// Very low rate, but no frame reduction below some frame_rate (8fps).
TEST_F(QmSelectTest, LimitDownTemporalAction) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(10, 8, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {10, 10, 10};
  int encoder_sent_rate[] = {10, 10, 10};
  int incoming_frame_rate[] = {8, 8, 8};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, medium spatial.
  UpdateQmContentData(0.01f, 0.03f, 0.03f, 0.03f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(2, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

// Two stages: spatial down-sample and then back up spatially,
// as rate as increased.
TEST_F(QmSelectTest, 2StageDownSpatialUpSpatial) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                    fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset and go up in rate: expected to go back up.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(320, 240);
  EXPECT_EQ(1, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {400, 400, 400, 400, 400};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 0, 0, 1));
}

// Two stages: spatial down-sample and then back up spatially, since encoder
// is under-shooting target even though rate has not increased much.
TEST_F(QmSelectTest, 2StageDownSpatialUpSpatialUndershoot) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                    fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset rates and simulate under-shooting scenario.: expect to go back up.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(320, 240);
  EXPECT_EQ(1, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {200, 200, 200, 200, 200};
  int encoder_sent_rate2[] = {50, 50, 50, 50, 50};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 0, 0, 1));
}

// Two stages: spatial down-sample and then no action to go up,
// as encoding rate mis-match is too high.
TEST_F(QmSelectTest, 2StageDownSpatialNoActionUp) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                    fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset and simulate large rate mis-match: expect no action to go back up.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(320, 240);
  EXPECT_EQ(1, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}
// Two stages: temporally down-sample and then back up temporally,
// as rate as increased.
TEST_F(QmSelectTest, 2StatgeDownTemporalUpTemporal) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                    fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  // Reset rates and go up in rate: expect to go back up.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {400, 400, 400, 400, 400};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 0));
}

// Two stages: temporal down-sample and then back up temporally, since encoder
// is under-shooting target even though rate has not increased much.
TEST_F(QmSelectTest, 2StatgeDownTemporalUpTemporalUndershoot) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                    fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  // Reset rates and simulate under-shooting scenario.: expect to go back up.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {200, 200, 200, 200, 200};
  int encoder_sent_rate2[] = {50, 50, 50, 50, 50};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 0));
}

// Two stages: temporal down-sample and then no action to go up,
// as encoding rate mis-match is too high.
TEST_F(QmSelectTest, 2StageDownTemporalNoActionUp) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  // Reset and simulate large rate mis-match: expect no action to go back up.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {600, 600, 600, 600, 600};
  int encoder_sent_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}
// 3 stages: spatial down-sample, followed by temporal down-sample,
// and then go up to full state, as encoding rate has increased.
TEST_F(QmSelectTest, 3StageDownSpatialTemporlaUpSpatialTemporal) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(100, 30, 640, 480);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(2, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset rate and change content data: expect temporal down-sample.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(320, 240);
  EXPECT_EQ(1, qm_resolution_->GetImageType(320, 240));

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(0.01f, 0.1f, 0.1f, 0.1f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2));

  // Reset rates and go high up in rate: expect to go back up both spatial
  // and temporally.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int encoder_sent_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 0, 0, 0));
}

// No down-sampling below some totol amount (factor of 16)
TEST_F(QmSelectTest, NoActionTooMuchDownSampling) {
  // Initialize with bitrate, frame rate, and native system width/height.
  InitQmNativeData(400, 30, 1280, 720);

  // Update with encoder frame size.
  uint16_t codec_width = 1280;
  uint16_t codec_height = 720;
  qm_resolution_->UpdateCodecFrameSize(codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {400, 400, 400};
  int encoder_sent_rate[] = {400, 400, 400};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial: 2x2 spatial expected.
  UpdateQmContentData(0.1f, 0.01f, 0.01f, 0.01f);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset and lower rates to get another spatial action.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(640, 360);
  EXPECT_EQ(2, qm_resolution_->GetImageType(640, 360));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {100, 100, 100, 100, 100};
  int encoder_sent_rate2[] = {100, 100, 100, 100, 100};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 2, 2, 1));

  // Reset and go to low rate: no action should be taken,
  // we went down too much already.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecFrameSize(320, 180);
  EXPECT_EQ(0, qm_resolution_->GetImageType(320, 180));
  // Update rates for a sequence of intervals.
  int target_rate3[] = {10, 10, 10, 10, 10};
  int encoder_sent_rate3[] = {10, 10, 10, 10, 10};
  int incoming_frame_rate3[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 1));
}

void QmSelectTest::InitQmNativeData(float initial_bit_rate,
                                    int user_frame_rate,
                                    int native_width,
                                    int native_height) {
  EXPECT_EQ(0, qm_resolution_->Initialize(initial_bit_rate, user_frame_rate,
                                          native_width, native_height));
}

void QmSelectTest::UpdateQmContentData(float motion_metric,
                                       float spatial_metric,
                                       float spatial_metric_horiz,
                                       float spatial_metric_vert) {
  content_metrics_->motion_magnitude = motion_metric;
  content_metrics_->spatial_pred_err = spatial_metric;
  content_metrics_->spatial_pred_err_h = spatial_metric_horiz;
  content_metrics_->spatial_pred_err_v = spatial_metric_vert;
  qm_resolution_->UpdateContent(content_metrics_);
}

void QmSelectTest::UpdateQmEncodedFrame(int* encoded_size, int num_updates) {
  FrameType frame_type = kVideoFrameDelta;
  for (int i = 0; i < num_updates; i++) {
    // Convert to bytes.
    int32_t encoded_size_update = 1000 * encoded_size[i] / 8;
    qm_resolution_->UpdateEncodedSize(encoded_size_update, frame_type);
  }
}

void QmSelectTest::UpdateQmRateData(int* target_rate,
                                    int* encoder_sent_rate,
                                    int* incoming_frame_rate,
                                    uint8_t* fraction_lost,
                                    int num_updates) {
  for (int i = 0; i < num_updates; i++) {
    float target_rate_update = target_rate[i];
    float encoder_sent_rate_update = encoder_sent_rate[i];
    float incoming_frame_rate_update = incoming_frame_rate[i];
    uint8_t fraction_lost_update = fraction_lost[i];
    qm_resolution_->UpdateRates(target_rate_update,
                                encoder_sent_rate_update,
                                incoming_frame_rate_update,
                                fraction_lost_update);
  }
}

// Check is the selected action from the QmResolution class is the same
// as the expected scales from |fac_width|, |fac_height|, |fac_temp|.
bool QmSelectTest::IsSelectedActionCorrect(VCMResolutionScale* qm_scale,
                                           uint8_t fac_width,
                                           uint8_t fac_height,
                                           uint8_t fac_temp) {
  if (qm_scale->spatialWidthFact == fac_width &&
      qm_scale->spatialHeightFact == fac_height &&
      qm_scale->temporalFact == fac_temp) {
    return true;
  } else {
    return false;
  }
}
}  // namespace webrtc
