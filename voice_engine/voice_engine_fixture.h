/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/include/fake_audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "voice_engine/include/voe_base.h"
#include "voice_engine/include/voe_network.h"

namespace webrtc {

class VoiceEngineFixture : public ::testing::Test {
 protected:
  VoiceEngineFixture();
  ~VoiceEngineFixture();

  VoiceEngine* voe_;
  VoEBase* base_;
  VoENetwork* network_;
  FakeAudioDeviceModule adm_;
  MockTransport transport_;
  rtc::scoped_refptr<AudioProcessing> apm_;
};

}  // namespace webrtc
