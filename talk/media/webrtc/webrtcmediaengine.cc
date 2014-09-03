/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#if defined(LIBPEERCONNECTION_LIB) || \
    defined(LIBPEERCONNECTION_IMPLEMENTATION)

#include "talk/media/webrtc/webrtcmediaengine.h"
#include "talk/media/webrtc/webrtcvideoengine.h"
#ifdef WEBRTC_CHROMIUM_BUILD
#include "talk/media/webrtc/webrtcvideoengine2.h"
#endif
#include "talk/media/webrtc/webrtcvoiceengine.h"
#ifdef WEBRTC_CHROMIUM_BUILD
#include "webrtc/system_wrappers/interface/field_trial.h"
#endif

namespace cricket {

class WebRtcMediaEngine :
      public CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine> {
 public:
  WebRtcMediaEngine() {}
  WebRtcMediaEngine(webrtc::AudioDeviceModule* adm,
      webrtc::AudioDeviceModule* adm_sc,
      WebRtcVideoEncoderFactory* encoder_factory,
      WebRtcVideoDecoderFactory* decoder_factory) {
    voice_.SetAudioDeviceModule(adm, adm_sc);
    video_.SetVoiceEngine(&voice_);
    video_.SetExternalEncoderFactory(encoder_factory);
    video_.SetExternalDecoderFactory(decoder_factory);
  }
};

#ifdef WEBRTC_CHROMIUM_BUILD
class WebRtcMediaEngine2 :
      public CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine2> {
 public:
  WebRtcMediaEngine2(webrtc::AudioDeviceModule* adm,
                     webrtc::AudioDeviceModule* adm_sc,
                     WebRtcVideoEncoderFactory* encoder_factory,
                     WebRtcVideoDecoderFactory* decoder_factory) {
    voice_.SetAudioDeviceModule(adm, adm_sc);
    video_.SetVoiceEngine(&voice_);
  }
};
#endif  // WEBRTC_CHROMIUM_BUILD

}  // namespace cricket

WRME_EXPORT
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
#ifdef WEBRTC_CHROMIUM_BUILD
  if (webrtc::field_trial::FindFullName("WebRTC-NewVideoAPI") == "Enabled") {
    return new cricket::WebRtcMediaEngine2(
        adm, adm_sc, encoder_factory, decoder_factory);
  }
#endif // WEBRTC_CHROMIUM_BUILD
  // This is just to get a diff to run pulse.
  return new cricket::WebRtcMediaEngine(
      adm, adm_sc, encoder_factory, decoder_factory);
}

WRME_EXPORT
void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
  delete media_engine;
}

namespace cricket {

// Used by ChannelManager when no media engine is passed in to it
// explicitly (acts as a default).
MediaEngineInterface* WebRtcMediaEngineFactory::Create() {
  return new cricket::WebRtcMediaEngine();
}

// Used by PeerConnectionFactory to create a media engine passed into
// ChannelManager.
MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    WebRtcVideoEncoderFactory* encoder_factory,
    WebRtcVideoDecoderFactory* decoder_factory) {
  return CreateWebRtcMediaEngine(adm, adm_sc, encoder_factory, decoder_factory);
}

}  // namespace cricket

#endif  // defined(LIBPEERCONNECTION_LIB) ||
        // defined(LIBPEERCONNECTION_IMPLEMENTATION)
