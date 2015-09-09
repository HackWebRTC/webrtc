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
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"

namespace webrtc {

namespace {
const CodecInst kOpusSettings = {105, "opus", 48000, 960, 1, 32000};
}  // namespace

class AudioEncoderOpusTest : public ::testing::Test {
 protected:
  void CreateCodec(int num_channels) {
    codec_inst_.channels = num_channels;
    encoder_.reset(new AudioEncoderOpus(codec_inst_));
    auto expected_app =
        num_channels == 1 ? AudioEncoderOpus::kVoip : AudioEncoderOpus::kAudio;
    EXPECT_EQ(expected_app, encoder_->application());
  }

  CodecInst codec_inst_ = kOpusSettings;
  rtc::scoped_ptr<AudioEncoderOpus> encoder_;
};

TEST_F(AudioEncoderOpusTest, DefaultApplicationModeMono) {
  CreateCodec(1);
}

TEST_F(AudioEncoderOpusTest, DefaultApplicationModeStereo) {
  CreateCodec(2);
}

TEST_F(AudioEncoderOpusTest, ChangeApplicationMode) {
  CreateCodec(2);
  EXPECT_TRUE(encoder_->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpus::kVoip, encoder_->application());
}

TEST_F(AudioEncoderOpusTest, ResetWontChangeApplicationMode) {
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

TEST_F(AudioEncoderOpusTest, ToggleDtx) {
  CreateCodec(2);
  // Enable DTX
  EXPECT_TRUE(encoder_->SetDtx(true));
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, encoder_->application());
  // Turn off DTX.
  EXPECT_TRUE(encoder_->SetDtx(false));
}

TEST_F(AudioEncoderOpusTest, SetBitrate) {
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

namespace {

// These constants correspond to those used in
// AudioEncoderOpus::SetProjectedPacketLossRate.
const double kPacketLossRate20 = 0.20;
const double kPacketLossRate10 = 0.10;
const double kPacketLossRate5 = 0.05;
const double kPacketLossRate1 = 0.01;
const double kLossRate20Margin = 0.02;
const double kLossRate10Margin = 0.01;
const double kLossRate5Margin = 0.01;

// Repeatedly sets packet loss rates in the range [from, to], increasing by
// 0.01 in each step. The function verifies that the actual loss rate is
// |expected_return|.
void TestSetPacketLossRate(AudioEncoderOpus* encoder,
                           double from,
                           double to,
                           double expected_return) {
  for (double loss = from; loss <= to;
       (to >= from) ? loss += 0.01 : loss -= 0.01) {
    encoder->SetProjectedPacketLossRate(loss);
    EXPECT_DOUBLE_EQ(expected_return, encoder->packet_loss_rate());
  }
}

}  // namespace

TEST_F(AudioEncoderOpusTest, PacketLossRateOptimized) {
  CreateCodec(1);

  // Note that the order of the following calls is critical.
  TestSetPacketLossRate(encoder_.get(), 0.0, 0.0, 0.0);
  TestSetPacketLossRate(encoder_.get(), kPacketLossRate1,
                        kPacketLossRate5 + kLossRate5Margin - 0.01,
                        kPacketLossRate1);
  TestSetPacketLossRate(encoder_.get(), kPacketLossRate5 + kLossRate5Margin,
                        kPacketLossRate10 + kLossRate10Margin - 0.01,
                        kPacketLossRate5);
  TestSetPacketLossRate(encoder_.get(), kPacketLossRate10 + kLossRate10Margin,
                        kPacketLossRate20 + kLossRate20Margin - 0.01,
                        kPacketLossRate10);
  TestSetPacketLossRate(encoder_.get(), kPacketLossRate20 + kLossRate20Margin,
                        1.0, kPacketLossRate20);
  TestSetPacketLossRate(encoder_.get(), kPacketLossRate20 + kLossRate20Margin,
                        kPacketLossRate20 - kLossRate20Margin,
                        kPacketLossRate20);
  TestSetPacketLossRate(
      encoder_.get(), kPacketLossRate20 - kLossRate20Margin - 0.01,
      kPacketLossRate10 - kLossRate10Margin, kPacketLossRate10);
  TestSetPacketLossRate(encoder_.get(),
                        kPacketLossRate10 - kLossRate10Margin - 0.01,
                        kPacketLossRate5 - kLossRate5Margin, kPacketLossRate5);
  TestSetPacketLossRate(encoder_.get(),
                        kPacketLossRate5 - kLossRate5Margin - 0.01,
                        kPacketLossRate1, kPacketLossRate1);
  TestSetPacketLossRate(encoder_.get(), 0.0, 0.0, 0.0);
}

}  // namespace webrtc
