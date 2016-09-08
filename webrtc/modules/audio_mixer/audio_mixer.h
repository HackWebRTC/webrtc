/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_

#include <memory>

#include "webrtc/modules/audio_mixer/audio_mixer_defines.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
class MixerAudioSource;

class AudioMixer {
 public:
  static const int kMaximumAmountOfMixedAudioSources = 3;
  enum Frequency {
    kNbInHz = 8000,
    kWbInHz = 16000,
    kSwbInHz = 32000,
    kFbInHz = 48000,
    kDefaultFrequency = kWbInHz
  };

  // Factory method. Constructor disabled.
  static std::unique_ptr<AudioMixer> Create(int id);
  virtual ~AudioMixer() {}

  // Add/remove audio sources as candidates for mixing.
  virtual int32_t SetMixabilityStatus(MixerAudioSource* audio_source,
                                      bool mixable) = 0;
  // Returns true if an audio source is a candidate for mixing.
  virtual bool MixabilityStatus(const MixerAudioSource& audio_source) const = 0;

  // Inform the mixer that the audio source should always be mixed and not
  // count toward the number of mixed audio sources. Note that an audio source
  // must have been added to the mixer (by calling SetMixabilityStatus())
  // before this function can be successfully called.
  virtual int32_t SetAnonymousMixabilityStatus(MixerAudioSource* audio_source,
                                               bool mixable) = 0;

  // Performs mixing by asking registered audio sources for audio. The
  // mixed result is placed in the provided AudioFrame. Can only be
  // called from a single thread. The rate and channels arguments
  // specify the rate and number of channels of the mix result.
  virtual void Mix(int sample_rate,
                   size_t number_of_channels,
                   AudioFrame* audio_frame_for_mixing) = 0;

  // Returns true if the audio source is mixed anonymously.
  virtual bool AnonymousMixabilityStatus(
      const MixerAudioSource& audio_source) const = 0;

  // Output level functions for VoEVolumeControl. Return value
  // between 0 and 9 is returned by voe::AudioLevel.
  virtual int GetOutputAudioLevel() = 0;

  // Return value between 0 and 0x7fff is returned by voe::AudioLevel.
  virtual int GetOutputAudioLevelFullRange() = 0;

 protected:
  AudioMixer() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioMixer);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_
