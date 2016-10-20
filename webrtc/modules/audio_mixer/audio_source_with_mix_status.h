/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_SOURCE_WITH_MIX_STATUS_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_SOURCE_WITH_MIX_STATUS_H_

#include "webrtc/api/audio/audio_mixer.h"

namespace webrtc {

// A class that holds a mixer participant and its mixing status.
class AudioSourceWithMixStatus {
 public:
  explicit AudioSourceWithMixStatus(AudioMixer::Source* audio_source);
  ~AudioSourceWithMixStatus();

  AudioSourceWithMixStatus(const AudioSourceWithMixStatus&) = default;

  // Returns true if the audio source was mixed this mix iteration.
  bool IsMixed() const;

  // Returns true if the audio source was mixed previous mix
  // iteration.
  bool WasMixed() const;

  // Updates the mixed status.
  void SetIsMixed(const bool mixed);
  void ResetMixedStatus();
  AudioMixer::Source* audio_source() const;

 private:
  AudioMixer::Source* audio_source_ = nullptr;
  bool is_mixed_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_SOURCE_WITH_MIX_STATUS_H_
