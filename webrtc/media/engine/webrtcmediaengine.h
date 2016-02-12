/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_

#include <string>
#include <vector>

#include "webrtc/config.h"
#include "webrtc/media/base/mediaengine.h"

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

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
