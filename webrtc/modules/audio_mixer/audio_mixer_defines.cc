/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/audio_mixer_defines.h"

namespace webrtc {

MixerAudioSource::MixerAudioSource() {}

MixerAudioSource::~MixerAudioSource() {}

bool MixerAudioSource::IsMixed() const {
  return is_mixed_;
}

bool MixerAudioSource::WasMixed() const {
  // Was mixed is the same as is mixed depending on perspective. This function
  // is for the perspective of AudioMixerImpl.
  return IsMixed();
}

int32_t MixerAudioSource::SetIsMixed(const bool mixed) {
  is_mixed_ = mixed;
  return 0;
}

void MixerAudioSource::ResetMixedStatus() {
  is_mixed_ = false;
}

}  // namespace webrtc
