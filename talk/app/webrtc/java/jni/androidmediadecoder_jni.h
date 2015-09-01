/*
 * libjingle
 * Copyright 2015 Google Inc.
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
 *
 */

#ifndef TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIADECODER_JNI_H_
#define TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIADECODER_JNI_H_

#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"

namespace webrtc_jni {

// Implementation of Android MediaCodec based decoder factory.
class MediaCodecVideoDecoderFactory
    : public cricket::WebRtcVideoDecoderFactory {
 public:
  MediaCodecVideoDecoderFactory();
  virtual ~MediaCodecVideoDecoderFactory();

  void SetEGLContext(JNIEnv* jni, jobject render_egl_context);

  // WebRtcVideoDecoderFactory implementation.
  webrtc::VideoDecoder* CreateVideoDecoder(webrtc::VideoCodecType type)
      override;

  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

 private:
  jobject render_egl_context_; // Render EGL context.
  std::vector<webrtc::VideoCodecType> supported_codec_types_;
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIADECODER_JNI_H_
