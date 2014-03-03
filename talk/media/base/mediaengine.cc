//
// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "talk/media/base/mediaengine.h"

namespace cricket {
const int MediaEngineInterface::kDefaultAudioDelayOffset = 0;
}

#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)

#if defined(HAVE_LINPHONE)
#include "talk/media/other/linphonemediaengine.h"
#endif  // HAVE_LINPHONE
#if defined(HAVE_WEBRTC_VOICE)
#include "talk/media/webrtc/webrtcvoiceengine.h"
#endif  // HAVE_WEBRTC_VOICE
#if defined(HAVE_WEBRTC_VIDEO)
#include "talk/media/webrtc/webrtcvideoengine.h"
#endif  // HAVE_WEBRTC_VIDEO

namespace cricket {
#if defined(HAVE_WEBRTC_VOICE)
#define AUDIO_ENG_NAME WebRtcVoiceEngine
#else
#define AUDIO_ENG_NAME NullVoiceEngine
#endif

#if defined(HAVE_WEBRTC_VIDEO)
template<>
CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine>::
    CompositeMediaEngine() {
  video_.SetVoiceEngine(&voice_);
}
#define VIDEO_ENG_NAME WebRtcVideoEngine
#endif

MediaEngineInterface* MediaEngineFactory::Create() {
#if defined(HAVE_LINPHONE)
  return new LinphoneMediaEngine("", "");
#elif defined(AUDIO_ENG_NAME) && defined(VIDEO_ENG_NAME)
  return new CompositeMediaEngine<AUDIO_ENG_NAME, VIDEO_ENG_NAME>();
#else
  return new NullMediaEngine();
#endif
}

};  // namespace cricket

#endif  // DISABLE_MEDIA_ENGINE_FACTORY
