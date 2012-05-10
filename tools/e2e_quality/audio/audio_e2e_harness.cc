/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Sets up a simple VoiceEngine loopback call with the default audio devices
// and runs forever. Some parameters can be configured through command-line
// flags.

#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "src/voice_engine/main/interface/voe_audio_processing.h"
#include "src/voice_engine/main/interface/voe_base.h"
#include "src/voice_engine/main/interface/voe_codec.h"

DEFINE_string(codec, "ISAC", "codec name");
DEFINE_int32(rate, 16000, "codec sample rate in Hz");

namespace webrtc {
namespace {

void RunHarness() {
  VoiceEngine* voe = VoiceEngine::Create();
  ASSERT_TRUE(voe != NULL);
  VoEAudioProcessing* audio = VoEAudioProcessing::GetInterface(voe);
  ASSERT_TRUE(audio != NULL);
  VoEBase* base = VoEBase::GetInterface(voe);
  ASSERT_TRUE(base != NULL);
  VoECodec* codec = VoECodec::GetInterface(voe);
  ASSERT_TRUE(codec != NULL);

  ASSERT_EQ(0, base->Init());
  int channel = base->CreateChannel();
  ASSERT_NE(-1, channel);
  ASSERT_EQ(0, base->SetSendDestination(channel, 1234, "127.0.0.1"));
  ASSERT_EQ(0, base->SetLocalReceiver(channel, 1234));

  CodecInst codec_params = {0};
  bool codec_found = false;
  for (int i = 0; i < codec->NumOfCodecs(); i++) {
    ASSERT_EQ(0, codec->GetCodec(i, codec_params));
    if (FLAGS_codec.compare(codec_params.plname) == 0 &&
        FLAGS_rate == codec_params.plfreq) {
      codec_found = true;
      break;
    }
  }
  ASSERT_TRUE(codec_found);
  ASSERT_EQ(0, codec->SetSendCodec(channel, codec_params));

  // Disable all audio processing.
  ASSERT_EQ(0, audio->SetAgcStatus(false));
  ASSERT_EQ(0, audio->SetEcStatus(false));
  ASSERT_EQ(0, audio->EnableHighPassFilter(false));
  ASSERT_EQ(0, audio->SetNsStatus(false));

  ASSERT_EQ(0, base->StartReceive(channel));
  ASSERT_EQ(0, base->StartPlayout(channel));
  ASSERT_EQ(0, base->StartSend(channel));

  // Run forever...
  while (1);
}

}  // namespace
}  // namespace webrtc

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::RunHarness();
}
