/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/media_jni.h"

#include "webrtc/api/audio_codecs/audio_decoder_factory.h"  // nogncheck
#include "webrtc/api/audio_codecs/audio_encoder_factory.h"  // nogncheck
#include "webrtc/call/callfactoryinterface.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"  // nogncheck
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"  // nogncheck

namespace webrtc_jni {

// This implementation is used for building WebRTC without audio and video
// support.
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
CreateNativePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    webrtc::AudioDeviceModule* default_adm,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreateModularPeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, nullptr /*audio_mixer*/,
      std::unique_ptr<cricket::MediaEngineInterface>(),
      std::unique_ptr<webrtc::CallFactoryInterface>(),
      std::unique_ptr<webrtc::RtcEventLogFactoryInterface>());
}

}  // namespace webrtc_jni
