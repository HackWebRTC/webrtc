/*
 * libjingle
 * Copyright 2016 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/media/webrtc/nullwebrtcvideoengine.h"
#include "webrtc/media/webrtc/webrtcvoiceengine.h"

namespace cricket {

class WebRtcMediaEngineNullVideo
    : public CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine> {
 public:
  WebRtcMediaEngineNullVideo(webrtc::AudioDeviceModule* adm,
                             WebRtcVideoEncoderFactory* encoder_factory,
                             WebRtcVideoDecoderFactory* decoder_factory) {
    voice_.SetAudioDeviceModule(adm);
    video_.SetExternalDecoderFactory(decoder_factory);
    video_.SetExternalEncoderFactory(encoder_factory);
  }
};

// Simple test to check if NullWebRtcVideoEngine implements the methods
// required by CompositeMediaEngine.
TEST(NullWebRtcVideoEngineTest, CheckInterface) {
  WebRtcMediaEngineNullVideo engine(nullptr, nullptr, nullptr);

  EXPECT_TRUE(engine.Init(rtc::Thread::Current()));
  engine.Terminate();
}

}  // namespace cricket
