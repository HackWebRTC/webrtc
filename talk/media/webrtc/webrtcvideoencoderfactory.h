/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTC_WEBRTCVIDEOENCODERFACTORY_H_
#define TALK_MEDIA_WEBRTC_WEBRTCVIDEOENCODERFACTORY_H_

#include "talk/media/base/codec.h"
#include "webrtc/base/refcount.h"
#include "webrtc/common_types.h"

namespace webrtc {
class VideoEncoder;
}

namespace cricket {

class WebRtcVideoEncoderFactory {
 public:
  struct VideoCodec {
    webrtc::VideoCodecType type;
    std::string name;
    int max_width;
    int max_height;
    int max_fps;

    VideoCodec(webrtc::VideoCodecType t, const std::string& nm, int w, int h,
               int fr)
        : type(t), name(nm), max_width(w), max_height(h), max_fps(fr) {
    }
  };

  virtual ~WebRtcVideoEncoderFactory() {}

  // Caller takes the ownership of the returned object and it should be released
  // by calling DestroyVideoEncoder().
  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) = 0;

  // Returns a list of supported codecs in order of preference.
  virtual const std::vector<VideoCodec>& codecs() const = 0;

  // Returns true if encoders created by this factory of the given codec type
  // will use internal camera sources, meaning that they don't require/expect
  // frames to be delivered via webrtc::VideoEncoder::Encode. This flag is used
  // as the internal_source parameter to
  // webrtc::ViEExternalCodec::RegisterExternalSendCodec.
  virtual bool EncoderTypeHasInternalSource(webrtc::VideoCodecType type) const {
    return false;
  }

  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) = 0;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENCODERFACTORY_H_
