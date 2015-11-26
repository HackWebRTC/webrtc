/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_encoder.h"
#include "webrtc/modules/audio_coding/acm2/codec_manager.h"

namespace webrtc {
namespace acm2 {

using ::testing::Return;

namespace {

// Create a MockAudioEncoder with some reasonable default behavior.
rtc::scoped_ptr<MockAudioEncoder> CreateMockEncoder() {
  auto enc = rtc_make_scoped_ptr(new MockAudioEncoder);
  EXPECT_CALL(*enc, SampleRateHz()).WillRepeatedly(Return(8000));
  EXPECT_CALL(*enc, NumChannels()).WillRepeatedly(Return(1));
  EXPECT_CALL(*enc, Max10MsFramesInAPacket()).WillRepeatedly(Return(1));
  EXPECT_CALL(*enc, Die());
  return enc;
}

}  // namespace

TEST(CodecManagerTest, ExternalEncoderFec) {
  auto enc0 = CreateMockEncoder();
  auto enc1 = CreateMockEncoder();
  {
    ::testing::InSequence s;
    EXPECT_CALL(*enc0, SetFec(false)).WillOnce(Return(true));
    EXPECT_CALL(*enc0, Mark("A"));
    EXPECT_CALL(*enc0, SetFec(true)).WillOnce(Return(true));
    EXPECT_CALL(*enc1, SetFec(true)).WillOnce(Return(true));
    EXPECT_CALL(*enc1, SetFec(false)).WillOnce(Return(true));
    EXPECT_CALL(*enc0, Mark("B"));
    EXPECT_CALL(*enc0, SetFec(false)).WillOnce(Return(true));
  }

  CodecManager cm;
  EXPECT_FALSE(cm.codec_fec_enabled());
  cm.RegisterEncoder(enc0.get());
  EXPECT_FALSE(cm.codec_fec_enabled());
  enc0->Mark("A");
  EXPECT_EQ(0, cm.SetCodecFEC(true));
  EXPECT_TRUE(cm.codec_fec_enabled());
  cm.RegisterEncoder(enc1.get());
  EXPECT_TRUE(cm.codec_fec_enabled());

  EXPECT_EQ(0, cm.SetCodecFEC(false));
  enc0->Mark("B");
  EXPECT_FALSE(cm.codec_fec_enabled());
  cm.RegisterEncoder(enc0.get());
  EXPECT_FALSE(cm.codec_fec_enabled());
}

}  // namespace acm2
}  // namespace webrtc
