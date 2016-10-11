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

#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {

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

  // A callback class that all mixer participants must inherit from/implement.
  class Source {
   public:
    enum class AudioFrameInfo {
      kNormal,  // The samples in audio_frame are valid and should be used.
      kMuted,   // The samples in audio_frame should not be used, but should be
      // implicitly interpreted as zero. Other fields in audio_frame
      // may be read and should contain meaningful values.
      kError  // audio_frame will not be used.
    };

    struct AudioFrameWithInfo {
      AudioFrame* audio_frame;
      AudioFrameInfo audio_frame_info;
    };

    // The implementation of GetAudioFrameWithInfo should update
    // audio_frame with new audio every time it's called. Implementing
    // classes are allowed to return the same AudioFrame pointer on
    // different calls. The pointer must stay valid until the next
    // mixing call or until this audio source is disconnected from the
    // mixer. The mixer may modify the contents of the passed
    // AudioFrame pointer at any time until the next call to
    // GetAudioFrameWithInfo, or until the source is removed from the
    // mixer.
    virtual AudioFrameWithInfo GetAudioFrameWithInfo(int32_t id,
                                                     int sample_rate_hz) = 0;

   protected:
    virtual ~Source() {}
  };

  // Factory method. Constructor disabled.
  static std::unique_ptr<AudioMixer> Create(int id);
  virtual ~AudioMixer() {}

  // Add/remove audio sources as candidates for mixing.
  virtual int32_t SetMixabilityStatus(Source* audio_source, bool mixable) = 0;
  // Returns true if an audio source is a candidate for mixing.
  virtual bool MixabilityStatus(const Source& audio_source) const = 0;

  // Inform the mixer that the audio source should always be mixed and not
  // count toward the number of mixed audio sources. Note that an audio source
  // must have been added to the mixer (by calling SetMixabilityStatus())
  // before this function can be successfully called.
  virtual int32_t SetAnonymousMixabilityStatus(Source* audio_source,
                                               bool mixable) = 0;

  // Performs mixing by asking registered audio sources for audio. The
  // mixed result is placed in the provided AudioFrame. Can only be
  // called from a single thread. The rate and channels arguments
  // specify the rate and number of channels of the mix result.
  virtual void Mix(int sample_rate,
                   size_t number_of_channels,
                   AudioFrame* audio_frame_for_mixing) = 0;

  // Returns true if the audio source is mixed anonymously.
  virtual bool AnonymousMixabilityStatus(const Source& audio_source) const = 0;

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
