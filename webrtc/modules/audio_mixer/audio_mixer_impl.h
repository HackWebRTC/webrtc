/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_mixer/audio_mixer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine_configurations.h"

namespace webrtc {

typedef std::vector<AudioFrame*> AudioFrameList;

class AudioMixerImpl : public AudioMixer {
 public:
  struct SourceStatus {
    SourceStatus(Source* audio_source, bool is_mixed, float gain)
        : audio_source(audio_source), is_mixed(is_mixed), gain(gain) {}
    Source* audio_source = nullptr;
    bool is_mixed = false;
    float gain = 0.0f;
  };

  typedef std::vector<SourceStatus> SourceStatusList;

  // AudioProcessing only accepts 10 ms frames.
  static const int kFrameDurationInMs = 10;

  static std::unique_ptr<AudioMixerImpl> Create(int id);

  ~AudioMixerImpl() override;

  // AudioMixer functions
  int32_t SetMixabilityStatus(Source* audio_source, bool mixable) override;
  bool MixabilityStatus(const Source& audio_source) const override;
  int32_t SetAnonymousMixabilityStatus(Source* audio_source,
                                       bool mixable) override;
  void Mix(int sample_rate,
           size_t number_of_channels,
           AudioFrame* audio_frame_for_mixing) override;
  bool AnonymousMixabilityStatus(const Source& audio_source) const override;

  // Returns true if the source was mixed last round. Returns
  // false and logs an error if the source was never added to the
  // mixer.
  bool GetAudioSourceMixabilityStatusForTest(Source* audio_source);

 private:
  AudioMixerImpl(int id, std::unique_ptr<AudioProcessing> limiter);

  // Set/get mix frequency
  int32_t SetOutputFrequency(const Frequency& frequency);
  Frequency OutputFrequency() const;

  // Compute what audio sources to mix from audio_source_list_. Ramp
  // in and out. Update mixed status. Mixes up to
  // kMaximumAmountOfMixedAudioSources audio sources.
  AudioFrameList GetNonAnonymousAudio() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Return the AudioFrames that should be mixed anonymously. Ramp in
  // and out. Update mixed status.
  AudioFrameList GetAnonymousAudio() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Add/remove the MixerAudioSource to the specified
  // MixerAudioSource list.
  bool AddAudioSourceToList(Source* audio_source,
                            SourceStatusList* audio_source_list) const;
  bool RemoveAudioSourceFromList(Source* remove_audio_source,
                                 SourceStatusList* audio_source_list) const;

  bool LimitMixedAudio(AudioFrame* mixed_audio) const;

  // Output level functions for VoEVolumeControl.
  int GetOutputAudioLevel() override;

  int GetOutputAudioLevelFullRange() override;

  rtc::CriticalSection crit_;

  const int32_t id_;

  // The current sample frequency and sample size when mixing.
  Frequency output_frequency_ ACCESS_ON(&thread_checker_);
  size_t sample_size_ ACCESS_ON(&thread_checker_);

  // List of all audio sources. Note all lists are disjunct
  SourceStatusList audio_source_list_ GUARDED_BY(crit_);  // May be mixed.

  // Always mixed, anonymously.
  SourceStatusList additional_audio_source_list_ GUARDED_BY(crit_);

  size_t num_mixed_audio_sources_ GUARDED_BY(crit_);
  // Determines if we will use a limiter for clipping protection during
  // mixing.
  bool use_limiter_ ACCESS_ON(&thread_checker_);

  uint32_t time_stamp_ ACCESS_ON(&thread_checker_);

  // Ensures that Mix is called from the same thread.
  rtc::ThreadChecker thread_checker_;

  // Used for inhibiting saturation in mixing.
  std::unique_ptr<AudioProcessing> limiter_ ACCESS_ON(&thread_checker_);

  // Measures audio level for the combined signal.
  voe::AudioLevel audio_level_ ACCESS_ON(&thread_checker_);

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioMixerImpl);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_IMPL_H_
