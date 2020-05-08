/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_ENCODER_H_
#define API_TEST_MOCK_VIDEO_ENCODER_H_

#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockEncodedImageCallback : public EncodedImageCallback {
 public:
  MOCK_METHOD(Result,
              OnEncodedImage,
              (const EncodedImage& encodedImage,
               const CodecSpecificInfo* codecSpecificInfo,
               const RTPFragmentationHeader* fragmentation),
              (override));
  MOCK_METHOD(void, OnDroppedFrame, (DropReason reason), (override));
};

class MockVideoEncoder : public VideoEncoder {
 public:
  MOCK_METHOD(void,
              SetFecControllerOverride,
              (FecControllerOverride * fec_controller_override),
              (override));
  MOCK_METHOD(int32_t,
              InitEncode,
              (const VideoCodec* codecSettings,
               int32_t numberOfCores,
               size_t maxPayloadSize),
              (override));
  MOCK_METHOD2(InitEncode,
               int32_t(const VideoCodec* codecSettings,
                       const VideoEncoder::Settings& settings));

  MOCK_METHOD2(Encode,
               int32_t(const VideoFrame& inputImage,
                       const std::vector<VideoFrameType>* frame_types));
  MOCK_METHOD1(RegisterEncodeCompleteCallback,
               int32_t(EncodedImageCallback* callback));
  MOCK_METHOD0(Release, int32_t());
  MOCK_METHOD0(Reset, int32_t());
  MOCK_METHOD1(SetRates, void(const RateControlParameters& parameters));
  MOCK_METHOD1(OnPacketLossRateUpdate, void(float packet_loss_rate));
  MOCK_METHOD1(OnRttUpdate, void(int64_t rtt_ms));
  MOCK_METHOD1(OnLossNotification,
               void(const LossNotification& loss_notification));
  MOCK_CONST_METHOD0(GetEncoderInfo, EncoderInfo(void));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_VIDEO_ENCODER_H_
