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

#include "talk/media/webrtc/webrtcmediaengine.h"
#include "webrtc/system_wrappers/interface/field_trial.h"

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
  return new cricket::WebRtcMediaEngine(
      adm, adm_sc, encoder_factory, decoder_factory);
}

WRME_EXPORT
void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
#ifdef WEBRTC_CHROMIUM_BUILD
  if (webrtc::field_trial::FindFullName("WebRTC-NewVideoAPI") == "Enabled") {
    delete static_cast<cricket::WebRtcMediaEngine2*>(media_engine);
  } else {
#endif // WEBRTC_CHROMIUM_BUILD
    delete static_cast<cricket::WebRtcMediaEngine*>(media_engine);
#ifdef WEBRTC_CHROMIUM_BUILD
  }
#endif // WEBRTC_CHROMIUM_BUILD
}
