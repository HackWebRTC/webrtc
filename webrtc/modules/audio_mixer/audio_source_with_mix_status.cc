/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/audio_source_with_mix_status.h"

namespace webrtc {

AudioSourceWithMixStatus::AudioSourceWithMixStatus(
    AudioMixer::Source* audio_source)
    : audio_source_(audio_source) {}

AudioSourceWithMixStatus::~AudioSourceWithMixStatus() {}

bool AudioSourceWithMixStatus::IsMixed() const {
  return is_mixed_;
}

bool AudioSourceWithMixStatus::WasMixed() const {
  // Was mixed is the same as is mixed depending on perspective. This function
  // is for the perspective of AudioMixerImpl.
  return IsMixed();
}

void AudioSourceWithMixStatus::SetIsMixed(const bool mixed) {
  is_mixed_ = mixed;
}

void AudioSourceWithMixStatus::ResetMixedStatus() {
  is_mixed_ = false;
}

AudioMixer::Source* AudioSourceWithMixStatus::audio_source() const {
  return audio_source_;
}

}  // namespace webrtc
