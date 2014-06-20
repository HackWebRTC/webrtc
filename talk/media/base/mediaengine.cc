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
#if defined(HAVE_LMI)
#include "talk/media/base/hybridvideoengine.h"
#include "talk/media/lmi/lmimediaengine.h"
#endif  // HAVE_LMI
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG

namespace cricket {
#if defined(HAVE_WEBRTC_VOICE)
#define AUDIO_ENG_NAME WebRtcVoiceEngine
#else
#define AUDIO_ENG_NAME NullVoiceEngine
#endif

#if defined(HAVE_WEBRTC_VIDEO)
#if !defined(HAVE_LMI)
template<>
CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine>::
    CompositeMediaEngine() {
  video_.SetVoiceEngine(&voice_);
}
#define VIDEO_ENG_NAME WebRtcVideoEngine
#else
// If we have both WebRtcVideoEngine and LmiVideoEngine, enable dual-stack.
// This small class here allows us to hook the WebRtcVideoChannel up to
// the capturer owned by the LMI engine, without infecting the rest of the
// HybridVideoEngine classes with this abstraction violation.
class WebRtcLmiHybridVideoEngine
    : public HybridVideoEngine<WebRtcVideoEngine, LmiVideoEngine> {
 public:
  void SetVoiceEngine(WebRtcVoiceEngine* engine) {
    video1_.SetVoiceEngine(engine);
  }
};
template<>
CompositeMediaEngine<WebRtcVoiceEngine, WebRtcLmiHybridVideoEngine>::
    CompositeMediaEngine() {
  video_.SetVoiceEngine(&voice_);
}
#define VIDEO_ENG_NAME WebRtcLmiHybridVideoEngine
#endif
#elif defined(HAVE_LMI)
#define VIDEO_ENG_NAME LmiVideoEngine
#else
#define VIDEO_ENG_NAME NullVideoEngine
#endif

MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::create_function_ = NULL;
MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::SetCreateFunction(MediaEngineCreateFunction function) {
  MediaEngineCreateFunction old_function = create_function_;
  create_function_ = function;
  return old_function;
};

MediaEngineInterface* MediaEngineFactory::Create() {
  if (create_function_) {
    return create_function_();
  } else {
#if defined(HAVE_LINPHONE)
    return new LinphoneMediaEngine("", "");
#elif defined(AUDIO_ENG_NAME) && defined(VIDEO_ENG_NAME)
    return new CompositeMediaEngine<AUDIO_ENG_NAME, VIDEO_ENG_NAME>();
#else
    return new NullMediaEngine();
#endif
  }
}

};  // namespace cricket

#endif  // DISABLE_MEDIA_ENGINE_FACTORY
