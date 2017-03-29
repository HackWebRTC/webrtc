/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/codecs/test/video_codec_test.h"

namespace webrtc {

class TestVp9Impl : public VideoCodecTest {
 protected:
  VideoEncoder* CreateEncoder() override { return VP9Encoder::Create(); }

  VideoDecoder* CreateDecoder() override { return VP9Decoder::Create(); }

  VideoCodec codec_settings() override {
    VideoCodec codec_inst;
    codec_inst.codecType = webrtc::kVideoCodecVP9;
    codec_inst.VP9()->numberOfTemporalLayers = 1;
    codec_inst.VP9()->numberOfSpatialLayers = 1;
    return codec_inst;
  }
};

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST_F(TestVp9Impl, DISABLED_EncodeDecode) {
#else
TEST_F(TestVp9Impl, EncodeDecode) {
#endif
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame_.get(), decoded_frame.get()), 36);
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame));
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

}  // namespace webrtc
