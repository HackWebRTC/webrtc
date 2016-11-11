/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

const int kMaxPayloadSize = 1024;

void SetDefaultSettings(VideoCodec* codec_settings) {
  codec_settings->codecType = kVideoCodecH264;
  codec_settings->maxFramerate = 60;
  codec_settings->width = 640;
  codec_settings->height = 480;
  codec_settings->H264()->packetization_mode = kH264PacketizationMode1;
  // If frame dropping is false, we get a warning that bitrate can't
  // be controlled for RC_QUALITY_MODE; RC_BITRATE_MODE and RC_TIMESTAMP_MODE
  codec_settings->H264()->frameDroppingOn = true;
  codec_settings->targetBitrate = 2000;
  codec_settings->maxBitrate = 4000;
}

TEST(H264EncoderImplTest, CanInitializeWithDefaultParameters) {
  H264EncoderImpl encoder;
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, 1, kMaxPayloadSize));
}

TEST(H264EncoderImplTest, CanInitializeWithPacketizationMode0) {
  H264EncoderImpl encoder;
  VideoCodec codec_settings;
  SetDefaultSettings(&codec_settings);
  codec_settings.H264()->packetization_mode = kH264PacketizationMode0;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder.InitEncode(&codec_settings, 1, kMaxPayloadSize));
}

}  // anonymous namespace

}  // namespace webrtc
