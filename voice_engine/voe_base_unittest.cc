/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/include/voe_base.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "test/gtest.h"
#include "voice_engine/channel_manager.h"
#include "voice_engine/shared_data.h"
#include "voice_engine/voice_engine_fixture.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {

class VoEBaseTest : public VoiceEngineFixture {};

TEST_F(VoEBaseTest, InitWithExternalAudioDevice) {
  EXPECT_EQ(0, base_->Init(&adm_, apm_.get()));
  EXPECT_EQ(0, base_->LastError());
}

TEST_F(VoEBaseTest, CreateChannelBeforeInitShouldFail) {
  int channelID = base_->CreateChannel();
  EXPECT_EQ(channelID, -1);
}

TEST_F(VoEBaseTest, CreateChannelAfterInit) {
  EXPECT_EQ(0, base_->Init(&adm_, apm_.get(), nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(0, base_->DeleteChannel(channelID));
}

TEST_F(VoEBaseTest, AssociateSendChannel) {
  EXPECT_EQ(0, base_->Init(&adm_, apm_.get()));

  const int channel_1 = base_->CreateChannel();

  // Associating with a channel that does not exist should fail.
  EXPECT_EQ(-1, base_->AssociateSendChannel(channel_1, channel_1 + 1));

  const int channel_2 = base_->CreateChannel();

  // Let the two channels associate with each other. This is not a normal use
  // case. Actually, circular association should be avoided in practice. This
  // is just to test that no crash is caused.
  EXPECT_EQ(0, base_->AssociateSendChannel(channel_1, channel_2));
  EXPECT_EQ(0, base_->AssociateSendChannel(channel_2, channel_1));

  voe::SharedData* shared_data = static_cast<voe::SharedData*>(
      static_cast<VoiceEngineImpl*>(voe_));
  voe::ChannelOwner reference = shared_data->channel_manager()
      .GetChannel(channel_1);
  EXPECT_EQ(0, base_->DeleteChannel(channel_1));
  // Make sure that the only use of the channel-to-delete is |reference|
  // at this point.
  EXPECT_EQ(1, reference.use_count());

  reference = shared_data->channel_manager().GetChannel(channel_2);
  EXPECT_EQ(0, base_->DeleteChannel(channel_2));
  EXPECT_EQ(1, reference.use_count());
}

TEST_F(VoEBaseTest, GetVersion) {
  char v1[1024] = {75};
  base_->GetVersion(v1);
  std::string v2 = VoiceEngine::GetVersionString() + "\n";
  EXPECT_EQ(v2, v1);
}
}  // namespace webrtc
