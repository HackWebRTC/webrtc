/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "after_streaming_fixture.h"

#include <cstring>

static const char* kLoopbackIp = "127.0.0.1";

AfterStreamingFixture::AfterStreamingFixture()
    : channel_(voe_base_->CreateChannel()) {
  EXPECT_GE(channel_, 0);

  const std::string& input_file = resource_manager_.long_audio_file_path();
  EXPECT_FALSE(input_file.empty());

  SetUpLocalPlayback();
  StartPlaying(input_file);
}

AfterStreamingFixture::~AfterStreamingFixture() {
  voe_file_->StopPlayingFileAsMicrophone(channel_);
  voe_base_->StopSend(channel_);
  voe_base_->StopPlayout(channel_);
  voe_base_->StopReceive(channel_);

  voe_base_->DeleteChannel(channel_);
}

void AfterStreamingFixture::SetUpLocalPlayback() {
  EXPECT_EQ(0, voe_base_->SetSendDestination(channel_, 8000, kLoopbackIp));
  EXPECT_EQ(0, voe_base_->SetLocalReceiver(0, 8000));

  webrtc::CodecInst codec;
  codec.channels = 1;
  codec.pacsize = 160;
  codec.plfreq = 8000;
  codec.pltype = 0;
  codec.rate = 64000;
  strcpy(codec.plname, "PCMU");

  voe_codec_->SetSendCodec(channel_, codec);
}

void AfterStreamingFixture::StartPlaying(const std::string& input_file) {
  EXPECT_EQ(0, voe_base_->StartReceive(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(
      channel_, input_file.c_str(), true, true));
}
