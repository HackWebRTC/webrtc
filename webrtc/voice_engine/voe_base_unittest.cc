/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/voice_engine/voice_engine_fixture.h"

namespace webrtc {

class VoEBaseTest : public VoiceEngineFixture {};

TEST_F(VoEBaseTest, InitWithExternalAudioDeviceAndAudioProcessing) {
  AudioProcessing* audioproc = AudioProcessing::Create();
  EXPECT_EQ(0, base_->Init(&adm_, audioproc));
  EXPECT_EQ(audioproc, base_->audio_processing());
  EXPECT_EQ(0, base_->LastError());
}

TEST_F(VoEBaseTest, InitWithExternalAudioDevice) {
  EXPECT_EQ(nullptr, base_->audio_processing());
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_NE(nullptr, base_->audio_processing());
  EXPECT_EQ(0, base_->LastError());
}

TEST_F(VoEBaseTest, CreateChannelBeforeInitShouldFail) {
  int channelID = base_->CreateChannel();
  EXPECT_EQ(channelID, -1);
}

TEST_F(VoEBaseTest, CreateChannelAfterInit) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(0, base_->DeleteChannel(channelID));
}

}  // namespace webrtc
