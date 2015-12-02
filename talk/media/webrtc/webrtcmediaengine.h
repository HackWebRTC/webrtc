/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCMEDIAENGINE_H_
#define TALK_MEDIA_WEBRTCMEDIAENGINE_H_

#include <string>
#include <vector>

#include "talk/media/base/mediaengine.h"
#include "webrtc/config.h"

namespace webrtc {
class AudioDeviceModule;
}
namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

namespace cricket {

class WebRtcMediaEngineFactory {
 public:
  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      WebRtcVideoEncoderFactory* encoder_factory,
      WebRtcVideoDecoderFactory* decoder_factory);
};

// Verify that extension IDs are within 1-byte extension range and are not
// overlapping.
bool ValidateRtpExtensions(const std::vector<RtpHeaderExtension>& extensions);

// Convert cricket::RtpHeaderExtension:s to webrtc::RtpExtension:s, discarding
// any extensions not validated by the 'supported' predicate. Duplicate
// extensions are removed if 'filter_redundant_extensions' is set, and also any
// mutually exclusive extensions (see implementation for details).
std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<RtpHeaderExtension>& extensions,
    bool (*supported)(const std::string&),
    bool filter_redundant_extensions);

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCMEDIAENGINE_H_
