/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/sdk/android/src/jni/pc/media_jni.h"

#include "webrtc/call/callfactoryinterface.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "webrtc/media/engine/webrtcmediaengine.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace jni {

CallFactoryInterface* CreateCallFactory() {
  return webrtc::CreateCallFactory().release();
}

RtcEventLogFactoryInterface* CreateRtcEventLogFactory() {
  return webrtc::CreateRtcEventLogFactory().release();
}

cricket::MediaEngineInterface* CreateMediaEngine(
    AudioDeviceModule* adm,
    const rtc::scoped_refptr<AudioEncoderFactory>& audio_encoder_factory,
    const rtc::scoped_refptr<AudioDecoderFactory>& audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer) {
  return cricket::WebRtcMediaEngineFactory::Create(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, AudioProcessing::Create());
}

}  // namespace jni
}  // namespace webrtc
