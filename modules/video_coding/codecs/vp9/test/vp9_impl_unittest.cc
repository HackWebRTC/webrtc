/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "test/video_codec_settings.h"

namespace webrtc {

namespace {
const size_t kWidth = 1280;
const size_t kHeight = 720;
}  // namespace

class TestVp9Impl : public VideoCodecUnitTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return VP9Encoder::Create();
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return VP9Decoder::Create();
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecVP9, codec_settings);
    codec_settings->width = kWidth;
    codec_settings->height = kHeight;
    codec_settings->VP9()->numberOfTemporalLayers = 1;
    codec_settings->VP9()->numberOfSpatialLayers = 1;
  }

  void ExpectFrameWith(uint8_t temporal_idx) {
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
    EXPECT_EQ(temporal_idx, codec_specific_info.codecSpecific.VP9.temporal_idx);
  }
};

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST_F(TestVp9Impl, DISABLED_EncodeDecode) {
#else
TEST_F(TestVp9Impl, EncodeDecode) {
#endif
  VideoFrame* input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame, decoded_frame.get()), 36);
}

// We only test the encoder here, since the decoded frame rotation is set based
// on the CVO RTP header extension in VCMDecodedFrameCallback::Decoded.
// TODO(brandtr): Consider passing through the rotation flag through the decoder
// in the same way as done in the encoder.
TEST_F(TestVp9Impl, EncodedRotationEqualsInputRotation) {
  VideoFrame* input_frame = NextInputFrame();
  input_frame->set_rotation(kVideoRotation_0);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_0, encoded_frame.rotation_);

  input_frame->set_rotation(kVideoRotation_90);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame, nullptr, nullptr));
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_90, encoded_frame.rotation_);
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

TEST_F(TestVp9Impl, ParserQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  int qp = 0;
  ASSERT_TRUE(vp9::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
  EXPECT_EQ(encoded_frame.qp_, qp);
}

TEST_F(TestVp9Impl, EncoderWith2TemporalLayers) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 2;
  // Tl0PidIdx is only used in non-flexible mode.
  codec_settings_.VP9()->flexibleMode = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(0, codec_specific_info.codecSpecific.VP9.temporal_idx);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(1);

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(0);

  // Temporal layer 1.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
  ExpectFrameWith(1);
}

TEST_F(TestVp9Impl, EncoderExplicitLayering) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 1;
  codec_settings_.VP9()->numberOfSpatialLayers = 2;

  codec_settings_.width = 960;
  codec_settings_.height = 540;
  codec_settings_.spatialLayers[0].minBitrate = 200;
  codec_settings_.spatialLayers[0].maxBitrate = 500;
  codec_settings_.spatialLayers[0].targetBitrate =
      (codec_settings_.spatialLayers[0].minBitrate +
       codec_settings_.spatialLayers[0].maxBitrate) /
      2;
  codec_settings_.spatialLayers[1].minBitrate = 400;
  codec_settings_.spatialLayers[1].maxBitrate = 1500;
  codec_settings_.spatialLayers[1].targetBitrate =
      (codec_settings_.spatialLayers[1].minBitrate +
       codec_settings_.spatialLayers[1].maxBitrate) /
      2;

  codec_settings_.spatialLayers[0].width = codec_settings_.width / 2;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Ensure it fails if scaling factors in horz/vert dimentions are different.
  codec_settings_.spatialLayers[0].width = codec_settings_.width;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 2;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Ensure it fails if scaling factor is not power of two.
  codec_settings_.spatialLayers[0].width = codec_settings_.width / 3;
  codec_settings_.spatialLayers[0].height = codec_settings_.height / 3;
  codec_settings_.spatialLayers[1].width = codec_settings_.width;
  codec_settings_.spatialLayers[1].height = codec_settings_.height;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));
}

TEST_F(TestVp9Impl, EnableDisableSpatialLayers) {
  // Configure encoder to produce N spatial layers. Encode few frames of layer 0
  // then enable layer 1 and encode few more frames and so on until layer N-1.
  // Then disable layers one by one in the same way.
  const size_t num_spatial_layers = 3;
  const size_t num_temporal_layers = 1;
  codec_settings_.VP9()->numberOfSpatialLayers =
      static_cast<unsigned char>(num_spatial_layers);
  codec_settings_.VP9()->numberOfTemporalLayers =
      static_cast<unsigned char>(num_temporal_layers);

  std::vector<SpatialLayer> layers =
      GetSvcConfig(codec_settings_.width, codec_settings_.height,
                   num_spatial_layers, num_temporal_layers);
  for (size_t i = 0; i < layers.size(); ++i) {
    codec_settings_.spatialLayers[i] = layers[i];
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  VideoBitrateAllocation bitrate_allocation;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    bitrate_allocation.SetBitrate(sl_idx, 0,
                                  layers[sl_idx].targetBitrate * 1000);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    const size_t num_frames_to_encode = 3;
    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx + 1);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    }
  }

  for (size_t i = 0; i < num_spatial_layers - 1; ++i) {
    const size_t sl_idx = num_spatial_layers - i - 1;
    bitrate_allocation.SetBitrate(sl_idx, 0, 0);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    const size_t num_frames_to_encode = 3;
    for (size_t frame_num = 0; frame_num < num_frames_to_encode; ++frame_num) {
      SetWaitForEncodedFramesThreshold(sl_idx);
      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
      std::vector<EncodedImage> encoded_frame;
      std::vector<CodecSpecificInfo> codec_specific_info;
      ASSERT_TRUE(WaitForEncodedFrames(&encoded_frame, &codec_specific_info));
    }
  }
}

TEST_F(TestVp9Impl, EndOfPicture) {
  const size_t num_spatial_layers = 2;
  const size_t num_temporal_layers = 1;
  codec_settings_.VP9()->numberOfSpatialLayers =
      static_cast<unsigned char>(num_spatial_layers);
  codec_settings_.VP9()->numberOfTemporalLayers =
      static_cast<unsigned char>(num_temporal_layers);

  std::vector<SpatialLayer> layers =
      GetSvcConfig(codec_settings_.width, codec_settings_.height,
                   num_spatial_layers, num_temporal_layers);
  for (size_t i = 0; i < layers.size(); ++i) {
    codec_settings_.spatialLayers[i] = layers[i];
  }

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Encode both base and upper layers. Check that end-of-superframe flag is
  // set on upper layer frame but not on base layer frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, layers[0].targetBitrate * 1000);
  bitrate_allocation.SetBitrate(1, 0, layers[1].targetBitrate * 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  SetWaitForEncodedFramesThreshold(2);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

  std::vector<EncodedImage> frames;
  std::vector<CodecSpecificInfo> codec_specific;
  ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));
  EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.end_of_picture);
  EXPECT_TRUE(codec_specific[1].codecSpecific.VP9.end_of_picture);

  // Encode only base layer. Check that end-of-superframe flag is
  // set on base layer frame.
  bitrate_allocation.SetBitrate(1, 0, 0);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_settings_.maxFramerate));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  SetWaitForEncodedFramesThreshold(1);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

  ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));
  EXPECT_EQ(codec_specific[0].codecSpecific.VP9.spatial_idx, 0);
  EXPECT_TRUE(codec_specific[0].codecSpecific.VP9.end_of_picture);
}

TEST_F(TestVp9Impl, InterLayerPred) {
  const size_t num_spatial_layers = 2;
  const size_t num_temporal_layers = 1;
  codec_settings_.VP9()->numberOfSpatialLayers =
      static_cast<unsigned char>(num_spatial_layers);
  codec_settings_.VP9()->numberOfTemporalLayers =
      static_cast<unsigned char>(num_temporal_layers);
  codec_settings_.VP9()->frameDroppingOn = false;

  std::vector<SpatialLayer> layers =
      GetSvcConfig(codec_settings_.width, codec_settings_.height,
                   num_spatial_layers, num_temporal_layers);

  BitrateAllocation bitrate_allocation;
  for (size_t i = 0; i < layers.size(); ++i) {
    codec_settings_.spatialLayers[i] = layers[i];
    bitrate_allocation.SetBitrate(i, 0, layers[i].targetBitrate * 1000);
  }

  const std::vector<InterLayerPredMode> inter_layer_pred_modes = {
      InterLayerPredMode::kOff, InterLayerPredMode::kOn,
      InterLayerPredMode::kOnKeyPic};

  for (const InterLayerPredMode inter_layer_pred : inter_layer_pred_modes) {
    codec_settings_.VP9()->interLayerPred = inter_layer_pred;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                   0 /* max payload size (unused) */));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->SetRateAllocation(bitrate_allocation,
                                          codec_settings_.maxFramerate));

    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));

    std::vector<EncodedImage> frames;
    std::vector<CodecSpecificInfo> codec_specific;
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    // Key frame.
    EXPECT_FALSE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.spatial_idx, 0);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred == InterLayerPredMode::kOff);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);

    SetWaitForEncodedFramesThreshold(2);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, nullptr));
    ASSERT_TRUE(WaitForEncodedFrames(&frames, &codec_specific));

    // Delta frame.
    EXPECT_TRUE(codec_specific[0].codecSpecific.VP9.inter_pic_predicted);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.spatial_idx, 0);
    EXPECT_EQ(codec_specific[0].codecSpecific.VP9.non_ref_for_inter_layer_pred,
              inter_layer_pred == InterLayerPredMode::kOff ||
                  inter_layer_pred == InterLayerPredMode::kOnKeyPic);
    EXPECT_TRUE(
        codec_specific[1].codecSpecific.VP9.non_ref_for_inter_layer_pred);
  }
}

}  // namespace webrtc
