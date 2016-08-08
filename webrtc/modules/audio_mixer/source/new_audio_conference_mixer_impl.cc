/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/source/new_audio_conference_mixer_impl.h"

#include <algorithm>
#include <functional>

#include "webrtc/modules/audio_conference_mixer/source/audio_frame_manipulator.h"
#include "webrtc/modules/audio_mixer/include/audio_mixer_defines.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/utility.h"

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
      energy_ = CalculateEnergy(*a);
    }
  }

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

// Mix |frame| into |mixed_frame|, with saturation protection and upmixing.
// These effects are applied to |frame| itself prior to mixing. Assumes that
// |mixed_frame| always has at least as many channels as |frame|. Supports
// stereo at most.
//
// TODO(andrew): consider not modifying |frame| here.
void MixFrames(AudioFrame* mixed_frame, AudioFrame* frame, bool use_limiter) {
  RTC_DCHECK_GE(mixed_frame->num_channels_, frame->num_channels_);
  if (use_limiter) {
    // Divide by two to avoid saturation in the mixing.
    // This is only meaningful if the limiter will be used.
    *frame >>= 1;
  }
  RTC_DCHECK_EQ(frame->num_channels_, mixed_frame->num_channels_);
  *mixed_frame += *frame;
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

NewAudioConferenceMixer* NewAudioConferenceMixer::Create(int id) {
  NewAudioConferenceMixerImpl* mixer = new NewAudioConferenceMixerImpl(id);
  if (!mixer->Init()) {
    delete mixer;
    return NULL;
  }
  return mixer;
}

NewAudioConferenceMixerImpl::NewAudioConferenceMixerImpl(int id)
    : id_(id),
      output_frequency_(kDefaultFrequency),
      sample_size_(0),
      audio_source_list_(),
      additional_audio_source_list_(),
      num_mixed_audio_sources_(0),
      use_limiter_(true),
      time_stamp_(0) {
  thread_checker_.DetachFromThread();
}

bool NewAudioConferenceMixerImpl::Init() {
  crit_.reset(CriticalSectionWrapper::CreateCriticalSection());
  if (crit_.get() == NULL)
    return false;

  cb_crit_.reset(CriticalSectionWrapper::CreateCriticalSection());
  if (cb_crit_.get() == NULL)
    return false;

  Config config;
  config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
  limiter_.reset(AudioProcessing::Create(config));
  if (!limiter_.get())
    return false;

  if (SetOutputFrequency(kDefaultFrequency) == -1)
    return false;

  if (limiter_->gain_control()->set_mode(GainControl::kFixedDigital) !=
      limiter_->kNoError)
    return false;

  // We smoothly limit the mixed frame to -7 dbFS. -6 would correspond to the
  // divide-by-2 but -7 is used instead to give a bit of headroom since the
  // AGC is not a hard limiter.
  if (limiter_->gain_control()->set_target_level_dbfs(7) != limiter_->kNoError)
    return false;

  if (limiter_->gain_control()->set_compression_gain_db(0) !=
      limiter_->kNoError)
    return false;

  if (limiter_->gain_control()->enable_limiter(true) != limiter_->kNoError)
    return false;

  if (limiter_->gain_control()->Enable(true) != limiter_->kNoError)
    return false;

  return true;
}

void NewAudioConferenceMixerImpl::Mix(int sample_rate,
                                      size_t number_of_channels,
                                      AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(number_of_channels == 1 || number_of_channels == 2);
  size_t remainingAudioSourcesAllowedToMix = kMaximumAmountOfMixedAudioSources;
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  AudioFrameList mixList;
  AudioFrameList additionalFramesList;
  std::map<int, MixerAudioSource*> mixedAudioSourcesMap;
  {
    CriticalSectionScoped cs(cb_crit_.get());
    Frequency mixing_frequency;

    switch (sample_rate) {
      case 8000:
        mixing_frequency = kNbInHz;
        break;
      case 16000:
        mixing_frequency = kWbInHz;
        break;
      case 32000:
        mixing_frequency = kSwbInHz;
        break;
      case 48000:
        mixing_frequency = kFbInHz;
        break;
      default:
        RTC_NOTREACHED();
        return;
    }

    if (OutputFrequency() != mixing_frequency) {
      SetOutputFrequency(mixing_frequency);
    }

    mixList = UpdateToMix(remainingAudioSourcesAllowedToMix);
    remainingAudioSourcesAllowedToMix -= mixList.size();
    GetAdditionalAudio(&additionalFramesList);
  }

  for (FrameAndMuteInfo& frame_and_mute : mixList) {
    RemixFrame(frame_and_mute.frame, number_of_channels);
  }
  for (FrameAndMuteInfo& frame_and_mute : additionalFramesList) {
    RemixFrame(frame_and_mute.frame, number_of_channels);
  }

  audio_frame_for_mixing->UpdateFrame(
      -1, time_stamp_, NULL, 0, output_frequency_, AudioFrame::kNormalSpeech,
      AudioFrame::kVadPassive, number_of_channels);

  time_stamp_ += static_cast<uint32_t>(sample_size_);

  use_limiter_ = num_mixed_audio_sources_ > 1 &&
                 output_frequency_ <= AudioProcessing::kMaxNativeSampleRateHz;

  // We only use the limiter if it supports the output sample rate and
  // we're actually mixing multiple streams.
  MixFromList(audio_frame_for_mixing, mixList, id_, use_limiter_);

  {
    CriticalSectionScoped cs(crit_.get());
    MixAnonomouslyFromList(audio_frame_for_mixing, additionalFramesList);

    if (audio_frame_for_mixing->samples_per_channel_ == 0) {
      // Nothing was mixed, set the audio samples to silence.
      audio_frame_for_mixing->samples_per_channel_ = sample_size_;
      audio_frame_for_mixing->Mute();
    } else {
      // Only call the limiter if we have something to mix.
      LimitMixedAudio(audio_frame_for_mixing);
    }
  }
  return;
}

int32_t NewAudioConferenceMixerImpl::SetOutputFrequency(
    const Frequency& frequency) {
  CriticalSectionScoped cs(crit_.get());

  output_frequency_ = frequency;
  sample_size_ =
      static_cast<size_t>((output_frequency_ * kProcessPeriodicityInMs) / 1000);

  return 0;
}

NewAudioConferenceMixer::Frequency
NewAudioConferenceMixerImpl::OutputFrequency() const {
  CriticalSectionScoped cs(crit_.get());
  return output_frequency_;
}

int32_t NewAudioConferenceMixerImpl::SetMixabilityStatus(
    MixerAudioSource* audio_source,
    bool mixable) {
  if (!mixable) {
    // Anonymous audio sources are in a separate list. Make sure that the
    // audio source is in the _audioSourceList if it is being mixed.
    SetAnonymousMixabilityStatus(audio_source, false);
  }
  size_t numMixedAudioSources;
  {
    CriticalSectionScoped cs(cb_crit_.get());
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
    numMixedAudioSources =
        numMixedNonAnonymous + additional_audio_source_list_.size();
  }
  // A MixerAudioSource was added or removed. Make sure the scratch
  // buffer is updated if necessary.
  // Note: The scratch buffer may only be updated in Process().
  CriticalSectionScoped cs(crit_.get());
  num_mixed_audio_sources_ = numMixedAudioSources;
  return 0;
}

bool NewAudioConferenceMixerImpl::MixabilityStatus(
    const MixerAudioSource& audio_source) const {
  CriticalSectionScoped cs(cb_crit_.get());
  return IsAudioSourceInList(audio_source, audio_source_list_);
}

int32_t NewAudioConferenceMixerImpl::SetAnonymousMixabilityStatus(
    MixerAudioSource* audio_source,
    bool anonymous) {
  CriticalSectionScoped cs(cb_crit_.get());
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

bool NewAudioConferenceMixerImpl::AnonymousMixabilityStatus(
    const MixerAudioSource& audio_source) const {
  CriticalSectionScoped cs(cb_crit_.get());
  return IsAudioSourceInList(audio_source, additional_audio_source_list_);
}

AudioFrameList NewAudioConferenceMixerImpl::UpdateToMix(
    size_t maxAudioFrameCounter) const {
  AudioFrameList result;
  std::vector<SourceFrame> audioSourceMixingDataList;

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
      if (!p.was_mixed_before_) {
        RampIn(*p.audio_frame_);
      }
      result.emplace_back(p.audio_frame_, false);
      is_mixed = true;
    }

    // Ramp out unmuted.
    if (p.was_mixed_before_ && !is_mixed) {
      RampOut(*p.audio_frame_);
      result.emplace_back(p.audio_frame_, false);
    }

    p.audio_source_->_mixHistory->SetIsMixed(is_mixed);
  }
  return result;
}

void NewAudioConferenceMixerImpl::GetAdditionalAudio(
    AudioFrameList* additionalFramesList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "GetAdditionalAudio(additionalFramesList)");
  // The GetAudioFrameWithMuted() callback may result in the audio source being
  // removed from additionalAudioFramesList_. If that happens it will
  // invalidate any iterators. Create a copy of the audio sources list such
  // that the list of participants can be traversed safely.
  MixerAudioSourceList additionalAudioSourceList;
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
    if (audio_frame->samples_per_channel_ == 0) {
      // Empty frame. Don't use it.
      continue;
    }
    additionalFramesList->push_back(FrameAndMuteInfo(
        audio_frame, ret == MixerAudioSource::AudioFrameInfo::kMuted));
  }
}

bool NewAudioConferenceMixerImpl::IsAudioSourceInList(
    const MixerAudioSource& audio_source,
    const MixerAudioSourceList& audioSourceList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "IsAudioSourceInList(audio_source,audioSourceList)");
  return std::find(audioSourceList.begin(), audioSourceList.end(),
                   &audio_source) != audioSourceList.end();
}

bool NewAudioConferenceMixerImpl::AddAudioSourceToList(
    MixerAudioSource* audio_source,
    MixerAudioSourceList* audioSourceList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "AddAudioSourceToList(audio_source, audioSourceList)");
  audioSourceList->push_back(audio_source);
  // Make sure that the mixed status is correct for new MixerAudioSource.
  audio_source->_mixHistory->ResetMixedStatus();
  return true;
}

bool NewAudioConferenceMixerImpl::RemoveAudioSourceFromList(
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

int32_t NewAudioConferenceMixerImpl::MixFromList(
    AudioFrame* mixedAudio,
    const AudioFrameList& audioFrameList,
    int32_t id,
    bool use_limiter) {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id,
               "MixFromList(mixedAudio, audioFrameList)");
  if (audioFrameList.empty())
    return 0;

  uint32_t position = 0;

  if (audioFrameList.size() == 1) {
    mixedAudio->timestamp_ = audioFrameList.front().frame->timestamp_;
    mixedAudio->elapsed_time_ms_ =
        audioFrameList.front().frame->elapsed_time_ms_;
  } else {
    // TODO(wu): Issue 3390.
    // Audio frame timestamp is only supported in one channel case.
    mixedAudio->timestamp_ = 0;
    mixedAudio->elapsed_time_ms_ = -1;
  }

  for (AudioFrameList::const_iterator iter = audioFrameList.begin();
       iter != audioFrameList.end(); ++iter) {
    if (position >= kMaximumAmountOfMixedAudioSources) {
      WEBRTC_TRACE(
          kTraceMemory, kTraceAudioMixerServer, id,
          "Trying to mix more than max amount of mixed audio sources:%d!",
          kMaximumAmountOfMixedAudioSources);
      // Assert and avoid crash
      RTC_NOTREACHED();
      position = 0;
    }
    if (!iter->muted) {
      MixFrames(mixedAudio, iter->frame, use_limiter);
    }

    position++;
  }

  return 0;
}

// TODO(andrew): consolidate this function with MixFromList.
int32_t NewAudioConferenceMixerImpl::MixAnonomouslyFromList(
    AudioFrame* mixedAudio,
    const AudioFrameList& audioFrameList) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, id_,
               "MixAnonomouslyFromList(mixedAudio, audioFrameList)");

  if (audioFrameList.empty())
    return 0;

  for (AudioFrameList::const_iterator iter = audioFrameList.begin();
       iter != audioFrameList.end(); ++iter) {
    if (!iter->muted) {
      MixFrames(mixedAudio, iter->frame, use_limiter_);
    }
  }
  return 0;
}

bool NewAudioConferenceMixerImpl::LimitMixedAudio(
    AudioFrame* mixedAudio) const {
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
}  // namespace webrtc
