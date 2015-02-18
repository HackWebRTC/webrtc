/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_generic_codec.h"

namespace webrtc {
namespace acm2 {

#ifdef WEBRTC_CODEC_OPUS
namespace {
const CodecInst kDefaultOpusCodecInst = {105, "opus", 48000, 960, 1, 32000};
const int kCngPt = 255;  // Not using CNG in this test.
const int kRedPt = 255;  // Not using RED in this test.
}  // namespace

class AcmGenericCodecOpusTest : public ::testing::Test {
 protected:
  AcmGenericCodecOpusTest() {
    acm_codec_params_ = {kDefaultOpusCodecInst, false, false, VADNormal};
  }

  void CreateCodec() {
    codec_wrapper_.reset(new ACMGenericCodecWrapper(
        acm_codec_params_.codec_inst, kCngPt, kCngPt, kCngPt, kCngPt,
        false /* enable RED */, kRedPt));
    ASSERT_TRUE(codec_wrapper_);
    ASSERT_EQ(0, codec_wrapper_->InitEncoder(&acm_codec_params_, true));
  }

  const AudioEncoderOpus* GetAudioEncoderOpus() {
    const AudioEncoderOpus* ptr = static_cast<const AudioEncoderOpus*>(
        codec_wrapper_->GetAudioEncoder());
    EXPECT_NE(nullptr, ptr);
    return ptr;
  }
  WebRtcACMCodecParams acm_codec_params_;
  scoped_ptr<ACMGenericCodecWrapper> codec_wrapper_;
};

TEST_F(AcmGenericCodecOpusTest, DefaultApplicationModeMono) {
  acm_codec_params_.codec_inst.channels = 1;
  CreateCodec();
  EXPECT_EQ(AudioEncoderOpus::kVoip, GetAudioEncoderOpus()->application());
}

TEST_F(AcmGenericCodecOpusTest, DefaultApplicationModeStereo) {
  acm_codec_params_.codec_inst.channels = 2;
  CreateCodec();
  EXPECT_EQ(AudioEncoderOpus::kAudio, GetAudioEncoderOpus()->application());
}

TEST_F(AcmGenericCodecOpusTest, ChangeApplicationMode) {
  // Create a stereo encoder.
  acm_codec_params_.codec_inst.channels = 2;
  CreateCodec();
  // Verify that the mode is kAudio.
  const AudioEncoderOpus* opus_ptr = GetAudioEncoderOpus();
  EXPECT_EQ(AudioEncoderOpus::kAudio, opus_ptr->application());

  // Change mode.
  EXPECT_EQ(0, codec_wrapper_->SetOpusApplication(kVoip));
  // Verify that the AudioEncoder object was changed.
  EXPECT_NE(opus_ptr, GetAudioEncoderOpus());
  EXPECT_EQ(AudioEncoderOpus::kVoip, GetAudioEncoderOpus()->application());
}

TEST_F(AcmGenericCodecOpusTest, ResetWontChangeApplicationMode) {
  // Create a stereo encoder.
  acm_codec_params_.codec_inst.channels = 2;
  CreateCodec();
  const AudioEncoderOpus* opus_ptr = GetAudioEncoderOpus();
  // Verify that the mode is kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, opus_ptr->application());

  // Trigger a reset.
  ASSERT_EQ(0, codec_wrapper_->InitEncoder(&acm_codec_params_, false));
  // Verify that the AudioEncoder object changed.
  EXPECT_NE(opus_ptr, GetAudioEncoderOpus());
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, GetAudioEncoderOpus()->application());

  // Now change to kVoip.
  EXPECT_EQ(0, codec_wrapper_->SetOpusApplication(kVoip));
  EXPECT_EQ(AudioEncoderOpus::kVoip, GetAudioEncoderOpus()->application());

  opus_ptr = GetAudioEncoderOpus();
  // Trigger a reset again.
  ASSERT_EQ(0, codec_wrapper_->InitEncoder(&acm_codec_params_, false));
  // Verify that the AudioEncoder object changed.
  EXPECT_NE(opus_ptr, GetAudioEncoderOpus());
  // Verify that the mode is still kVoip.
  EXPECT_EQ(AudioEncoderOpus::kVoip, GetAudioEncoderOpus()->application());
}
#endif  // WEBRTC_CODEC_OPUS

}  // namespace acm2
}  // namespace webrtc
