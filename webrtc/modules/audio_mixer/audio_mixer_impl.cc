/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_mixer/audio_frame_manipulator.h"
#include "webrtc/modules/audio_mixer/audio_mixer_defines.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace {

class SourceFrame {
 public:
  SourceFrame(MixerAudioSource* p, AudioFrame* a, bool m, bool was_mixed_before)
      : audio_source_(p),
        audio_frame_(a),
        muted_(m),
        was_mixed_before_(was_mixed_before) {
    if (!muted_) {
      energy_ = NewMixerCalculateEnergy(*a);
    }
  }

  SourceFrame(MixerAudioSource* p,
              AudioFrame* a,
              bool m,
              bool was_mixed_before,
              uint32_t energy)
      : audio_source_(p),
        audio_frame_(a),
        muted_(m),
        energy_(energy),
        was_mixed_before_(was_mixed_before) {}

  // a.shouldMixBefore(b) is used to select mixer participants.
  bool shouldMixBefore(const SourceFrame& other) const {
    if (muted_ != other.muted_) {
      return other.muted_;
    }

    auto our_activity = audio_frame_->vad_activity_;
    auto other_activity = other.audio_frame_->vad_activity_;

    if (our_activity != other_activity) {
      return our_activity == AudioFrame::kVadActive;
    }

    return energy_ > other.energy_;
  }

  MixerAudioSource* audio_source_;
  AudioFrame* audio_frame_;
  bool muted_;
  uint32_t energy_;
  bool was_mixed_before_;
};

// Remixes a frame between stereo and mono.
void RemixFrame(AudioFrame* frame, size_t number_of_channels) {
  RTC_DCHECK(number_of_channels == 1 || number_of_channels == 2);
  if (frame->num_channels_ == 1 && number_of_channels == 2) {
    AudioFrameOperations::MonoToStereo(frame);
  } else if (frame->num_channels_ == 2 && number_of_channels == 1) {
    AudioFrameOperations::StereoToMono(frame);
  }
}

void Ramp(const std::vector<SourceFrame>& mixed_sources_and_frames) {
  for (const auto& source_frame : mixed_sources_and_frames) {
    // Ramp in previously unmixed.
    if (!source_frame.was_mixed_before_) {
      NewMixerRampIn(source_frame.audio_frame_);
    }

    const bool is_mixed = source_frame.audio_source_->_mixHistory->IsMixed();
    // Ramp out currently unmixed.
    if (source_frame.was_mixed_before_ && !is_mixed) {
      NewMixerRampOut(source_frame.audio_frame_);
    }
  }
}

}  // namespace

MixerAudioSource::MixerAudioSource() : _mixHistory(new NewMixHistory()) {}

MixerAudioSource::~MixerAudioSource() {
  delete _mixHistory;
}

bool MixerAudioSource::IsMixed() const {
  return _mixHistory->IsMixed();
}

NewMixHistory::NewMixHistory() : is_mixed_(0) {}

NewMixHistory::~NewMixHistory() {}

bool NewMixHistory::IsMixed() const {
  return is_mixed_;
}

bool NewMixHistory::WasMixed() const {
  // Was mixed is the same as is mixed depending on perspective. This function
  // is for the perspective of NewAudioConferenceMixerImpl.
  return IsMixed();
}

int32_t NewMixHistory::SetIsMixed(const bool mixed) {
  is_mixed_ = mixed;
  return 0;
}

void NewMixHistory::ResetMixedStatus() {
  is_mixed_ = false;
}

std::unique_ptr<AudioMixer> AudioMixer::Create(int id) {
  return AudioMixerImpl::Create(id);
}

AudioMixerImpl::AudioMixerImpl(int id, std::unique_ptr<AudioProcessing> limiter)
    : id_(id),
      audio_source_list_(),
      additional_audio_source_list_(),
      num_mixed_audio_sources_(0),
      use_limiter_(true),
      time_stamp_(0),
      limiter_(std::move(limiter)) {
  SetOutputFrequency(kDefaultFrequency);
  thread_checker_.DetachFromThread();
}

AudioMixerImpl::~AudioMixerImpl() {}

std::unique_ptr<AudioMixer> AudioMixerImpl::Create(int id) {
  Config config;
  config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
  std::unique_ptr<AudioProcessing> limiter(AudioProcessing::Create(config));
  if (!limiter.get())
    return nullptr;

  if (limiter->gain_control()->set_mode(GainControl::kFixedDigital) !=
      limiter->kNoError)
    return nullptr;

  // We smoothly limit the mixed frame to -7 dbFS. -6 would correspond to the
  // divide-by-2 but -7 is used instead to give a bit of headroom since the
  // AGC is not a hard limiter.
  if (limiter->gain_control()->set_target_level_dbfs(7) != limiter->kNoError)
    return nullptr;

  if (limiter->gain_control()->set_compression_gain_db(0) != limiter->kNoError)
    return nullptr;

  if (limiter->gain_control()->enable_limiter(true) != limiter->kNoError)
    return nullptr;

  if (limiter->gain_control()->Enable(true) != limiter->kNoError)
    return nullptr;

  return std::unique_ptr<AudioMixer>(
      new AudioMixerImpl(id, std::move(limiter)));
}

void AudioMixerImpl::Mix(int sample_rate,
                         size_t number_of_channels,
                         AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(number_of_channels == 1 || number_of_channels == 2);
  RTC_DCHECK_RUN_ON(&thread_checker_);
  std::map<int, MixerAudioSource*> mixedAudioSourcesMap;

  if (sample_rate != kNbInHz && sample_rate != kWbInHz &&
      sample_rate != kSwbInHz && sample_rate != kFbInHz) {
    WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, id_,
                 "Invalid frequency: %d", sample_rate);
    RTC_NOTREACHED();
    return;
  }

  if (OutputFrequency() != sample_rate) {
    SetOutputFrequency(static_cast<Frequency>(sample_rate));
  }

  AudioFrameList mix_list;
  AudioFrameList anonymous_mix_list;
  int num_mixed_audio_sources;
  {
    rtc::CritScope lock(&crit_);
    mix_list = GetNonAnonymousAudio();
    anonymous_mix_list = GetAnonymousAudio();
    num_mixed_audio_sources = static_cast<int>(num_mixed_audio_sources_);
  }

  mix_list.insert(mix_list.begin(), anonymous_mix_list.begin(),
                  anonymous_mix_list.end());

  for (const auto& frame : mix_list) {
    RemixFrame(frame, number_of_channels);
  }

  audio_frame_for_mixing->UpdateFrame(
      -1, time_stamp_, NULL, 0, output_frequency_, AudioFrame::kNormalSpeech,
      AudioFrame::kVadPassive, number_of_channels);

  time_stamp_ += static_cast<uint32_t>(sample_size_);

  use_limiter_ = num_mixed_audio_sources > 1;

  // We only use the limiter if we're actually mixing multiple streams.
  MixFromList(audio_frame_for_mixing, mix_list, id_, use_limiter_);

  if (audio_frame_for_mixing->samples_per_channel_ == 0) {
    // Nothing was mixed, set the audio samples to silence.
    audio_frame_for_mixing->samples_per_channel_ = sample_size_;
    audio_frame_for_mixing->Mute();
  } else {
    // Only call the limiter if we have something to mix.
    LimitMixedAudio(audio_frame_for_mixing);
  }

  // Pass the final result to the level indicator.
  audio_level_.ComputeLevel(*audio_frame_for_mixing);

  return;
}

int32_t AudioMixerImpl::SetOutputFrequency(const Frequency& frequency) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  output_frequency_ = frequency;
  sample_size_ =
      static_cast<size_t>((output_frequency_ * kFrameDurationInMs) / 1000);

  return 0;
}

AudioMixer::Frequency AudioMixerImpl::OutputFrequency() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return output_frequency_;
}

int32_t AudioMixerImpl::SetMixabilityStatus(MixerAudioSource* audio_source,
                                            bool mixable) {
  if (!mixable) {
    // Anonymous audio sources are in a separate list. Make sure that the
    // audio source is in the _audioSourceList if it is being mixed.
    SetAnonymousMixabilityStatus(audio_source, false);
  }
  {
    rtc::CritScope lock(&crit_);
    const bool isMixed = IsAudioSourceInList(*audio_source, audio_source_list_);
    // API must be called with a new state.
    if (!(mixable ^ isMixed)) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, id_,
                   "Mixable is aready %s", isMixed ? "ON" : "off");
      return -1;
    }
    bool success = false;
    if (mixable) {
      success = AddAudioSourceToList(audio_source, &audio_source_list_);
    } else {
      success = RemoveAudioSourceFromList(audio_source, &audio_source_list_);
    }
    if (!success) {
      WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, id_,
                   "failed to %s audio_source", mixable ? "add" : "remove");
      RTC_NOTREACHED();
      return -1;
    }

    size_t numMixedNonAnonymous = audio_source_list_.size();
    if (numMixedNonAnonymous > kMaximumAmountOfMixedAudioSources) {
      numMixedNonAnonymous = kMaximumAmountOfMixedAudioSources;
    }
    num_mixed_audio_sources_ =
        numMixedNonAnonymous + additional_audio_source_list_.size();
  }
  return 0;
}

bool AudioMixerImpl::MixabilityStatus(
    const MixerAudioSource& audio_source) const {
  rtc::CritScope lock(&crit_);
  return IsAudioSourceInList(audio_source, audio_source_list_);
}

int32_t AudioMixerImpl::SetAnonymousMixabilityStatus(
    MixerAudioSource* audio_source,
    bool anonymous) {
  rtc::CritScope lock(&crit_);
  if (IsAudioSourceInList(*audio_source, additional_audio_source_list_)) {
    if (anonymous) {
      return 0;
    }
    if (!RemoveAudioSourceFromList(audio_source,
                                   &additional_audio_source_list_)) {
      WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, id_,
                   "unable to remove audio_source from anonymous list");
      RTC_NOTREACHED();
      return -1;
    }
    return AddAudioSourceToList(audio_source, &audio_source_list_) ? 0 : -1;
  }
  if (!anonymous) {
    return 0;
  }
  const bool mixable =
      RemoveAudioSourceFromList(audio_source, &audio_source_list_);
  if (!mixable) {
    WEBRTC_TRACE(
        kTraceWarning, kTraceAudioMixerServer, id_,
        "audio_source must be registered before turning it into anonymous");
    // Setting anonymous status is only possible if MixerAudioSource is
    // already registered.
    return -1;
  }
  return AddAudioSourceToList(audio_source, &additional_audio_source_list_)
             ? 0
             : -1;
}

bool AudioMixerImpl::AnonymousMixabilityStatus(
    const MixerAudioSource& audio_source) const {
  rtc::CritScope lock(&crit_);
  return IsAudioSourceInList(audio_source, additional_audio_source_list_);
}

AudioFrameList AudioMixerImpl::GetNonAnonymousAudio() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "GetNonAnonymousAudio()");
  AudioFrameList result;
  std::vector<SourceFrame> audioSourceMixingDataList;
  std::vector<SourceFrame> ramp_list;

  // Get audio source audio and put it in the struct vector.
  for (MixerAudioSource* audio_source : audio_source_list_) {
    auto audio_frame_with_info = audio_source->GetAudioFrameWithMuted(
        id_, static_cast<int>(output_frequency_));

    auto audio_frame_info = audio_frame_with_info.audio_frame_info;
    AudioFrame* audio_source_audio_frame = audio_frame_with_info.audio_frame;

    if (audio_frame_info == MixerAudioSource::AudioFrameInfo::kError) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, id_,
                   "failed to GetAudioFrameWithMuted() from participant");
      continue;
    }
    audioSourceMixingDataList.emplace_back(
        audio_source, audio_source_audio_frame,
        audio_frame_info == MixerAudioSource::AudioFrameInfo::kMuted,
        audio_source->_mixHistory->WasMixed());
  }

  // Sort frames by sorting function.
  std::sort(audioSourceMixingDataList.begin(), audioSourceMixingDataList.end(),
            std::mem_fn(&SourceFrame::shouldMixBefore));

  int maxAudioFrameCounter = kMaximumAmountOfMixedAudioSources;
  // Go through list in order and put things in mixList.
  for (SourceFrame& p : audioSourceMixingDataList) {
    // Filter muted.
    if (p.muted_) {
      p.audio_source_->_mixHistory->SetIsMixed(false);
      continue;
    }

    // Add frame to result vector for mixing.
    bool is_mixed = false;
    if (maxAudioFrameCounter > 0) {
      --maxAudioFrameCounter;
      result.push_back(p.audio_frame_);
      ramp_list.emplace_back(p.audio_source_, p.audio_frame_, false,
                             p.was_mixed_before_, -1);
      is_mixed = true;
    }
    p.audio_source_->_mixHistory->SetIsMixed(is_mixed);
  }
  Ramp(ramp_list);
  return result;
}

AudioFrameList AudioMixerImpl::GetAnonymousAudio() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "GetAnonymousAudio()");
  // The GetAudioFrameWithMuted() callback may result in the audio source being
  // removed from additionalAudioFramesList_. If that happens it will
  // invalidate any iterators. Create a copy of the audio sources list such
  // that the list of participants can be traversed safely.
  std::vector<SourceFrame> ramp_list;
  MixerAudioSourceList additionalAudioSourceList;
  AudioFrameList result;
  additionalAudioSourceList.insert(additionalAudioSourceList.begin(),
                                   additional_audio_source_list_.begin(),
                                   additional_audio_source_list_.end());

  for (MixerAudioSourceList::const_iterator audio_source =
           additionalAudioSourceList.begin();
       audio_source != additionalAudioSourceList.end(); ++audio_source) {
    auto audio_frame_with_info =
        (*audio_source)->GetAudioFrameWithMuted(id_, output_frequency_);
    auto ret = audio_frame_with_info.audio_frame_info;
    AudioFrame* audio_frame = audio_frame_with_info.audio_frame;
    if (ret == MixerAudioSource::AudioFrameInfo::kError) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, id_,
                   "failed to GetAudioFrameWithMuted() from audio_source");
      continue;
    }
    if (ret != MixerAudioSource::AudioFrameInfo::kMuted) {
      result.push_back(audio_frame);
      ramp_list.emplace_back(*audio_source, audio_frame, false,
                             (*audio_source)->_mixHistory->IsMixed(), -1);
      (*audio_source)->_mixHistory->SetIsMixed(true);
    }
  }
  Ramp(ramp_list);
  return result;
}

bool AudioMixerImpl::IsAudioSourceInList(
    const MixerAudioSource& audio_source,
    const MixerAudioSourceList& audioSourceList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "IsAudioSourceInList(audio_source,audioSourceList)");
  return std::find(audioSourceList.begin(), audioSourceList.end(),
                   &audio_source) != audioSourceList.end();
}

bool AudioMixerImpl::AddAudioSourceToList(
    MixerAudioSource* audio_source,
    MixerAudioSourceList* audioSourceList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "AddAudioSourceToList(audio_source, audioSourceList)");
  audioSourceList->push_back(audio_source);
  // Make sure that the mixed status is correct for new MixerAudioSource.
  audio_source->_mixHistory->ResetMixedStatus();
  return true;
}

bool AudioMixerImpl::RemoveAudioSourceFromList(
    MixerAudioSource* audio_source,
    MixerAudioSourceList* audioSourceList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "RemoveAudioSourceFromList(audio_source, audioSourceList)");
  auto iter =
      std::find(audioSourceList->begin(), audioSourceList->end(), audio_source);
  if (iter != audioSourceList->end()) {
    audioSourceList->erase(iter);
    // AudioSource is no longer mixed, reset to default.
    audio_source->_mixHistory->ResetMixedStatus();
    return true;
  } else {
    return false;
  }
}

int32_t AudioMixerImpl::MixFromList(AudioFrame* mixedAudio,
                                    const AudioFrameList& audioFrameList,
                                    int32_t id,
                                    bool use_limiter) {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id,
               "MixFromList(mixedAudio, audioFrameList)");
  if (audioFrameList.empty())
    return 0;

  uint32_t position = 0;

  if (audioFrameList.size() == 1) {
    mixedAudio->timestamp_ = audioFrameList.front()->timestamp_;
    mixedAudio->elapsed_time_ms_ = audioFrameList.front()->elapsed_time_ms_;
  } else {
    // TODO(wu): Issue 3390.
    // Audio frame timestamp is only supported in one channel case.
    mixedAudio->timestamp_ = 0;
    mixedAudio->elapsed_time_ms_ = -1;
  }

  for (const auto& frame : audioFrameList) {
    RTC_DCHECK_EQ(mixedAudio->sample_rate_hz_, frame->sample_rate_hz_);
    RTC_DCHECK_EQ(
        frame->samples_per_channel_,
        static_cast<size_t>((mixedAudio->sample_rate_hz_ * kFrameDurationInMs) /
                            1000));

    // Mix |f.frame| into |mixedAudio|, with saturation protection.
    // These effect is applied to |f.frame| itself prior to mixing.
    if (use_limiter) {
      // Divide by two to avoid saturation in the mixing.
      // This is only meaningful if the limiter will be used.
      *frame >>= 1;
    }
    RTC_DCHECK_EQ(frame->num_channels_, mixedAudio->num_channels_);
    *mixedAudio += *frame;
    position++;
  }
  return 0;
}

bool AudioMixerImpl::LimitMixedAudio(AudioFrame* mixedAudio) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!use_limiter_) {
    return true;
  }

  // Smoothly limit the mixed frame.
  const int error = limiter_->ProcessStream(mixedAudio);

  // And now we can safely restore the level. This procedure results in
  // some loss of resolution, deemed acceptable.
  //
  // It's possible to apply the gain in the AGC (with a target level of 0 dbFS
  // and compression gain of 6 dB). However, in the transition frame when this
  // is enabled (moving from one to two audio sources) it has the potential to
  // create discontinuities in the mixed frame.
  //
  // Instead we double the frame (with addition since left-shifting a
  // negative value is undefined).
  *mixedAudio += *mixedAudio;

  if (error != limiter_->kNoError) {
    WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, id_,
                 "Error from AudioProcessing: %d", error);
    RTC_NOTREACHED();
    return false;
  }
  return true;
}

int AudioMixerImpl::GetOutputAudioLevel() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  const int level = audio_level_.Level();
  WEBRTC_TRACE(kTraceStateInfo, kTraceAudioMixerServer, id_,
               "GetAudioOutputLevel() => level=%d", level);
  return level;
}

int AudioMixerImpl::GetOutputAudioLevelFullRange() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  const int level = audio_level_.LevelFullRange();
  WEBRTC_TRACE(kTraceStateInfo, kTraceAudioMixerServer, id_,
               "GetAudioOutputLevelFullRange() => level=%d", level);
  return level;
}
}  // namespace webrtc
