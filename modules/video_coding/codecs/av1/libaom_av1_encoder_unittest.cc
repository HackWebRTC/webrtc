/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <memory>

#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(LibaomAv1EncoderTest, CanCreate) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  EXPECT_TRUE(encoder);
}

TEST(LibaomAv1EncoderTest, InitAndRelease) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  ASSERT_TRUE(encoder);
  VideoCodec codec_settings;
  codec_settings.width = 1280;
  codec_settings.height = 720;
  codec_settings.maxFramerate = 30;
  VideoEncoder::Capabilities capabilities(/*loss_notification=*/false);
  VideoEncoder::Settings encoder_settings(capabilities, /*number_of_cores=*/1,
                                          /*max_payload_size=*/1200);
  EXPECT_EQ(encoder->InitEncode(&codec_settings, encoder_settings),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

}  // namespace
}  // namespace webrtc
