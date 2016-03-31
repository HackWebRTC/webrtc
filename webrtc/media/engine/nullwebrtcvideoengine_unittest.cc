/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/media/engine/nullwebrtcvideoengine.h"
#include "webrtc/media/engine/webrtcvoiceengine.h"

namespace cricket {

class WebRtcMediaEngineNullVideo
    : public CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine> {
 public:
  WebRtcMediaEngineNullVideo(webrtc::AudioDeviceModule* adm,
                             WebRtcVideoEncoderFactory* encoder_factory,
                             WebRtcVideoDecoderFactory* decoder_factory)
      : CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine>(adm) {
    video_.SetExternalDecoderFactory(decoder_factory);
    video_.SetExternalEncoderFactory(encoder_factory);
  }
};

// Simple test to check if NullWebRtcVideoEngine implements the methods
// required by CompositeMediaEngine.
TEST(NullWebRtcVideoEngineTest, CheckInterface) {
  WebRtcMediaEngineNullVideo engine(nullptr, nullptr, nullptr);
  EXPECT_TRUE(engine.Init());
}

}  // namespace cricket
