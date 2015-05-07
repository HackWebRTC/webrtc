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
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"

namespace webrtc {
namespace acm2 {

#ifdef WEBRTC_CODEC_OPUS
namespace {
const CodecInst kDefaultOpusCodecInst = {105, "opus", 48000, 960, 1, 32000};
}  // namespace

class AudioEncoderMutableOpusTest : public ::testing::Test {
 protected:
  AudioEncoderMutableOpusTest() : codec_inst_(kDefaultOpusCodecInst) {}

  void CreateCodec(int num_channels) {
    codec_inst_.channels = num_channels;
    encoder_.reset(new AudioEncoderMutableOpus(codec_inst_));
    auto expected_app =
        num_channels == 1 ? AudioEncoderOpus::kVoip : AudioEncoderOpus::kAudio;
    EXPECT_EQ(expected_app, encoder_->application());
  }

  CodecInst codec_inst_;
  rtc::scoped_ptr<AudioEncoderMutableOpus> encoder_;
};

TEST_F(AudioEncoderMutableOpusTest, DefaultApplicationModeMono) {
  CreateCodec(1);
}

TEST_F(AudioEncoderMutableOpusTest, DefaultApplicationModeStereo) {
  CreateCodec(2);
}

TEST_F(AudioEncoderMutableOpusTest, ChangeApplicationMode) {
  CreateCodec(2);
  EXPECT_TRUE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationSpeech, false));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());
}

TEST_F(AudioEncoderMutableOpusTest, ResetWontChangeApplicationMode) {
  CreateCodec(2);

  // Trigger a reset.
  encoder_->Reset();
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, encoder_->application());

  // Now change to kVoip.
  EXPECT_TRUE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationSpeech, false));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());

  // Trigger a reset again.
  encoder_->Reset();
  // Verify that the mode is still kVoip.
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());
}

TEST_F(AudioEncoderMutableOpusTest, ToggleDtx) {
  CreateCodec(2);

  // DTX is not allowed in audio mode, if mode forcing flag is false.
  EXPECT_FALSE(encoder_->SetDtx(true, false));
  EXPECT_EQ(AudioEncoderOpus::kAudio, encoder_->application());

  // DTX will be on, if mode forcing flag is true. Then application mode is
  // switched to kVoip.
  EXPECT_TRUE(encoder_->SetDtx(true, true));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());

  // Audio mode is not allowed when DTX is on, and DTX forcing flag is false.
  EXPECT_FALSE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationAudio, false));
  EXPECT_TRUE(encoder_->dtx_enabled());

  // Audio mode will be set, if DTX forcing flag is true. Then DTX is switched
  // off.
  EXPECT_TRUE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationAudio, true));
  EXPECT_FALSE(encoder_->dtx_enabled());

  // Now we set VOIP mode. The DTX forcing flag has no effect.
  EXPECT_TRUE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationSpeech, true));
  EXPECT_FALSE(encoder_->dtx_enabled());

  // In VOIP mode, we can enable DTX with mode forcing flag being false.
  EXPECT_TRUE(encoder_->SetDtx(true, false));

  // Turn off DTX.
  EXPECT_TRUE(encoder_->SetDtx(false, false));

  // When DTX is off, we can set Audio mode with DTX forcing flag being false.
  EXPECT_TRUE(
      encoder_->SetApplication(AudioEncoderMutable::kApplicationAudio, false));
}
#endif  // WEBRTC_CODEC_OPUS

}  // namespace acm2
}  // namespace webrtc
