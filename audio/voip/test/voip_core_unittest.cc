/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/voip_core.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr int kPcmuPayload = 0;

class VoipCoreTest : public ::testing::Test {
 public:
  const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

  VoipCoreTest() { audio_device_ = test::MockAudioDeviceModule::CreateNice(); }

  void SetUp() override {
    auto encoder_factory = CreateBuiltinAudioEncoderFactory();
    auto decoder_factory = CreateBuiltinAudioDecoderFactory();
    rtc::scoped_refptr<AudioProcessing> audio_processing =
        new rtc::RefCountedObject<test::MockAudioProcessing>();

    voip_core_ = std::make_unique<VoipCore>();
    voip_core_->Init(std::move(encoder_factory), std::move(decoder_factory),
                     CreateDefaultTaskQueueFactory(), audio_device_,
                     std::move(audio_processing));
  }

  std::unique_ptr<VoipCore> voip_core_;
  NiceMock<MockTransport> transport_;
  rtc::scoped_refptr<test::MockAudioDeviceModule> audio_device_;
};

// Validate expected API calls that involves with VoipCore. Some verification is
// involved with checking mock audio device.
TEST_F(VoipCoreTest, BasicVoipCoreOperation) {
  // Program mock as non-operational and ready to start.
  EXPECT_CALL(*audio_device_, Recording()).WillOnce(Return(false));
  EXPECT_CALL(*audio_device_, Playing()).WillOnce(Return(false));
  EXPECT_CALL(*audio_device_, InitRecording()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device_, InitPlayout()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device_, StartRecording()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device_, StartPlayout()).WillOnce(Return(0));

  auto channel = voip_core_->CreateChannel(&transport_, 0xdeadc0de);
  EXPECT_TRUE(channel);

  voip_core_->SetSendCodec(*channel, kPcmuPayload, kPcmuFormat);
  voip_core_->SetReceiveCodecs(*channel, {{kPcmuPayload, kPcmuFormat}});

  EXPECT_TRUE(voip_core_->StartSend(*channel));
  EXPECT_TRUE(voip_core_->StartPlayout(*channel));

  // Program mock as operational that is ready to be stopped.
  EXPECT_CALL(*audio_device_, Recording()).WillOnce(Return(true));
  EXPECT_CALL(*audio_device_, Playing()).WillOnce(Return(true));
  EXPECT_CALL(*audio_device_, StopRecording()).WillOnce(Return(0));
  EXPECT_CALL(*audio_device_, StopPlayout()).WillOnce(Return(0));

  EXPECT_TRUE(voip_core_->StopSend(*channel));
  EXPECT_TRUE(voip_core_->StopPlayout(*channel));
  voip_core_->ReleaseChannel(*channel);
}

TEST_F(VoipCoreTest, ExpectFailToUseReleasedChannelId) {
  auto channel = voip_core_->CreateChannel(&transport_, 0xdeadc0de);
  EXPECT_TRUE(channel);

  // Release right after creation.
  voip_core_->ReleaseChannel(*channel);

  // Now use released channel.

  // These should be no-op.
  voip_core_->SetSendCodec(*channel, kPcmuPayload, kPcmuFormat);
  voip_core_->SetReceiveCodecs(*channel, {{kPcmuPayload, kPcmuFormat}});

  EXPECT_FALSE(voip_core_->StartSend(*channel));
  EXPECT_FALSE(voip_core_->StartPlayout(*channel));
}

}  // namespace
}  // namespace webrtc
