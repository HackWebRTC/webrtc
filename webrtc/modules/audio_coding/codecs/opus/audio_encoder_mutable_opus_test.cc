/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(kwiberg): Merge these tests into audio_encoder_opus_unittest.cc

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
    encoder_.reset(new AudioEncoderOpus(codec_inst_));
    auto expected_app =
        num_channels == 1 ? AudioEncoderOpus::kVoip : AudioEncoderOpus::kAudio;
    EXPECT_EQ(expected_app, encoder_->application());
  }

  CodecInst codec_inst_;
  rtc::scoped_ptr<AudioEncoderOpus> encoder_;
};

TEST_F(AudioEncoderMutableOpusTest, DefaultApplicationModeMono) {
  CreateCodec(1);
}

TEST_F(AudioEncoderMutableOpusTest, DefaultApplicationModeStereo) {
  CreateCodec(2);
}

TEST_F(AudioEncoderMutableOpusTest, ChangeApplicationMode) {
  CreateCodec(2);
  EXPECT_TRUE(encoder_->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());
}

TEST_F(AudioEncoderMutableOpusTest, ResetWontChangeApplicationMode) {
  CreateCodec(2);

  // Trigger a reset.
  encoder_->Reset();
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, encoder_->application());

  // Now change to kVoip.
  EXPECT_TRUE(encoder_->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());

  // Trigger a reset again.
  encoder_->Reset();
  // Verify that the mode is still kVoip.
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());
}

TEST_F(AudioEncoderMutableOpusTest, ToggleDtx) {
  CreateCodec(2);
  // Enable DTX
  EXPECT_TRUE(encoder_->SetDtx(true));
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, encoder_->application());
  // Turn off DTX.
  EXPECT_TRUE(encoder_->SetDtx(false));
}

TEST_F(AudioEncoderMutableOpusTest, SetBitrate) {
  CreateCodec(1);
  // Constants are replicated from audio_encoder_opus.cc.
  const int kMinBitrateBps = 500;
  const int kMaxBitrateBps = 512000;
  // Set a too low bitrate.
  encoder_->SetTargetBitrate(kMinBitrateBps - 1);
  EXPECT_EQ(kMinBitrateBps, encoder_->GetTargetBitrate());
  // Set a too high bitrate.
  encoder_->SetTargetBitrate(kMaxBitrateBps + 1);
  EXPECT_EQ(kMaxBitrateBps, encoder_->GetTargetBitrate());
  // Set the minimum rate.
  encoder_->SetTargetBitrate(kMinBitrateBps);
  EXPECT_EQ(kMinBitrateBps, encoder_->GetTargetBitrate());
  // Set the maximum rate.
  encoder_->SetTargetBitrate(kMaxBitrateBps);
  EXPECT_EQ(kMaxBitrateBps, encoder_->GetTargetBitrate());
  // Set rates from 1000 up to 32000 bps.
  for (int rate = 1000; rate <= 32000; rate += 1000) {
    encoder_->SetTargetBitrate(rate);
    EXPECT_EQ(rate, encoder_->GetTargetBitrate());
  }
}
#endif  // WEBRTC_CODEC_OPUS

}  // namespace acm2
}  // namespace webrtc
