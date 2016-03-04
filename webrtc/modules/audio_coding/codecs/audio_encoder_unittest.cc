/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_encoder.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace webrtc {

TEST(AudioEncoderTest, EncodeInternalRedirectsOk) {
  const size_t kPayloadSize = 16;
  const uint8_t payload[kPayloadSize] =
      {0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,
       0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0};

  MockAudioEncoderDeprecated old_impl;
  MockAudioEncoder new_impl;
  MockAudioEncoderBase* impls[] = { &old_impl, &new_impl };
  for (auto* impl : impls) {
    EXPECT_CALL(*impl, Die());
    EXPECT_CALL(*impl, MaxEncodedBytes())
        .WillRepeatedly(Return(kPayloadSize * 2));
    EXPECT_CALL(*impl, NumChannels()).WillRepeatedly(Return(1));
    EXPECT_CALL(*impl, SampleRateHz()).WillRepeatedly(Return(8000));
  }

  EXPECT_CALL(old_impl, EncodeInternal(_, _, _, _)).WillOnce(
      Invoke(MockAudioEncoderDeprecated::CopyEncoding(payload)));

  EXPECT_CALL(new_impl, EncodeImpl(_, _, _)).WillOnce(
      Invoke(MockAudioEncoder::CopyEncoding(payload)));

  int16_t audio[80];
  uint8_t output_array[kPayloadSize * 2];
  rtc::Buffer output_buffer;

  AudioEncoder* old_encoder = &old_impl;
  AudioEncoder* new_encoder = &new_impl;
  auto old_info = old_encoder->Encode(0, audio, &output_buffer);
  auto new_info = new_encoder->DEPRECATED_Encode(0, audio,
                                                 kPayloadSize * 2,
                                                 output_array);

  EXPECT_EQ(old_info.encoded_bytes, kPayloadSize);
  EXPECT_EQ(new_info.encoded_bytes, kPayloadSize);
  EXPECT_EQ(output_buffer.size(), kPayloadSize);

  for (size_t i = 0; i != kPayloadSize; ++i) {
    EXPECT_EQ(output_buffer.data()[i], payload[i]);
    EXPECT_EQ(output_array[i], payload[i]);
  }
}

}  // namespace webrtc
