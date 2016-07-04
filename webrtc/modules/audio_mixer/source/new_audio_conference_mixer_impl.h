/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_SOURCE_NEW_AUDIO_CONFERENCE_MIXER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_MIXER_SOURCE_NEW_AUDIO_CONFERENCE_MIXER_IMPL_H_

#include <list>
#include <map>
#include <memory>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_mixer/include/new_audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/source/memory_pool.h"
#include "webrtc/modules/audio_conference_mixer/source/time_scheduler.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
class AudioProcessing;
class CriticalSectionWrapper;

struct FrameAndMuteInfo {
  FrameAndMuteInfo(AudioFrame* f, bool m) : frame(f), muted(m) {}
  AudioFrame* frame;
  bool muted;
};

typedef std::list<FrameAndMuteInfo> AudioFrameList;
typedef std::list<MixerAudioSource*> MixerAudioSourceList;

// Cheshire cat implementation of MixerAudioSource's non virtual functions.
class NewMixHistory {
 public:
  NewMixHistory();
  ~NewMixHistory();

  // Returns true if the participant is being mixed.
  bool IsMixed() const;

  // Returns true if the participant was mixed previous mix
  // iteration.
  bool WasMixed() const;

  // Updates the mixed status.
  int32_t SetIsMixed(bool mixed);

  void ResetMixedStatus();

 private:
  bool _isMixed;
};

class NewAudioConferenceMixerImpl : public NewAudioConferenceMixer {
 public:
  // AudioProcessing only accepts 10 ms frames.
  enum { kProcessPeriodicityInMs = 10 };

  explicit NewAudioConferenceMixerImpl(int id);
  ~NewAudioConferenceMixerImpl();

  // Must be called after ctor.
  bool Init();

  // Module functions
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // NewAudioConferenceMixer functions
  int32_t RegisterMixedStreamCallback(
      OldAudioMixerOutputReceiver* mixReceiver) override;
  int32_t UnRegisterMixedStreamCallback() override;
  int32_t SetMixabilityStatus(MixerAudioSource* participant,
                              bool mixable) override;
  bool MixabilityStatus(const MixerAudioSource& participant) const override;
  int32_t SetMinimumMixingFrequency(Frequency freq) override;
  int32_t SetAnonymousMixabilityStatus(MixerAudioSource* participant,
                                       bool mixable) override;
  bool AnonymousMixabilityStatus(
      const MixerAudioSource& participant) const override;

 private:
  enum { DEFAULT_AUDIO_FRAME_POOLSIZE = 50 };

  // Set/get mix frequency
  int32_t SetOutputFrequency(const Frequency& frequency);
  Frequency OutputFrequency() const;

  // Fills mixList with the AudioFrames pointers that should be used when
  // mixing.
  // maxAudioFrameCounter both input and output specifies how many more
  // AudioFrames that are allowed to be mixed.
  // rampOutList contain AudioFrames corresponding to an audio stream that
  // used to be mixed but shouldn't be mixed any longer. These AudioFrames
  // should be ramped out over this AudioFrame to avoid audio discontinuities.
  void UpdateToMix(AudioFrameList* mixList,
                   AudioFrameList* rampOutList,
                   std::map<int, MixerAudioSource*>* mixParticipantList,
                   size_t* maxAudioFrameCounter) const;

  // Return the lowest mixing frequency that can be used without having to
  // downsample any audio.
  int32_t GetLowestMixingFrequency() const;
  int32_t GetLowestMixingFrequencyFromList(
      const MixerAudioSourceList& mixList) const;

  // Return the AudioFrames that should be mixed anonymously.
  void GetAdditionalAudio(AudioFrameList* additionalFramesList) const;

  // Update the NewMixHistory of all MixerAudioSources. mixedParticipantsList
  // should contain a map of MixerAudioSources that have been mixed.
  void UpdateMixedStatus(
      const std::map<int, MixerAudioSource*>& mixedParticipantsList) const;

  // Clears audioFrameList and reclaims all memory associated with it.
  void ClearAudioFrameList(AudioFrameList* audioFrameList) const;

  // This function returns true if it finds the MixerAudioSource in the
  // specified list of MixerAudioSources.
  bool IsParticipantInList(const MixerAudioSource& participant,
                           const MixerAudioSourceList& participantList) const;

  // Add/remove the MixerAudioSource to the specified
  // MixerAudioSource list.
  bool AddParticipantToList(MixerAudioSource* participant,
                            MixerAudioSourceList* participantList) const;
  bool RemoveParticipantFromList(MixerAudioSource* removeParticipant,
                                 MixerAudioSourceList* participantList) const;

  // Mix the AudioFrames stored in audioFrameList into mixedAudio.
  int32_t MixFromList(AudioFrame* mixedAudio,
                      const AudioFrameList& audioFrameList) const;

  // Mix the AudioFrames stored in audioFrameList into mixedAudio. No
  // record will be kept of this mix (e.g. the corresponding MixerAudioSources
  // will not be marked as IsMixed()
  int32_t MixAnonomouslyFromList(AudioFrame* mixedAudio,
                                 const AudioFrameList& audioFrameList) const;

  bool LimitMixedAudio(AudioFrame* mixedAudio) const;

  std::unique_ptr<CriticalSectionWrapper> _crit;
  std::unique_ptr<CriticalSectionWrapper> _cbCrit;

  int32_t _id;

  Frequency _minimumMixingFreq;

  // Mix result callback
  OldAudioMixerOutputReceiver* _mixReceiver;

  // The current sample frequency and sample size when mixing.
  Frequency _outputFrequency;
  size_t _sampleSize;

  // Memory pool to avoid allocating/deallocating AudioFrames
  MemoryPool<AudioFrame>* _audioFramePool;

  // List of all participants. Note all lists are disjunct
  MixerAudioSourceList _participantList;  // May be mixed.
  // Always mixed, anonomously.
  MixerAudioSourceList _additionalParticipantList;

  size_t _numMixedParticipants;
  // Determines if we will use a limiter for clipping protection during
  // mixing.
  bool use_limiter_;

  uint32_t _timeStamp;

  // Metronome class.
  TimeScheduler _timeScheduler;

  // Counter keeping track of concurrent calls to process.
  // Note: should never be higher than 1 or lower than 0.
  int16_t _processCalls;

  // Used for inhibiting saturation in mixing.
  std::unique_ptr<AudioProcessing> _limiter;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_SOURCE_NEW_AUDIO_CONFERENCE_MIXER_IMPL_H_
