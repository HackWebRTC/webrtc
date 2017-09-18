/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/output_mixer.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/format_macros.h"
#include "system_wrappers/include/trace.h"
#include "voice_engine/statistics.h"
#include "voice_engine/utility.h"

namespace webrtc {
namespace voe {

void
OutputMixer::NewMixedAudio(int32_t id,
                           const AudioFrame& generalAudioFrame,
                           const AudioFrame** uniqueAudioFrames,
                           uint32_t size)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::NewMixedAudio(id=%d, size=%u)", id, size);

    _audioFrame.CopyFrom(generalAudioFrame);
    _audioFrame.id_ = id;
}

int32_t
OutputMixer::Create(OutputMixer*& mixer, uint32_t instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
                 "OutputMixer::Create(instanceId=%d)", instanceId);
    mixer = new OutputMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
                     "OutputMixer::Create() unable to allocate memory for"
                     "mixer");
        return -1;
    }
    return 0;
}

OutputMixer::OutputMixer(uint32_t instanceId) :
    _mixerModule(*AudioConferenceMixer::Create(instanceId)),
    _instanceId(instanceId),
    _mixingFrequencyHz(8000)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::OutputMixer() - ctor");

    if (_mixerModule.RegisterMixedStreamCallback(this) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "OutputMixer::OutputMixer() failed to register mixer"
                     "callbacks");
    }
}

void
OutputMixer::Destroy(OutputMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

OutputMixer::~OutputMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::~OutputMixer() - dtor");
    _mixerModule.UnRegisterMixedStreamCallback();
    delete &_mixerModule;
}

int32_t
OutputMixer::SetEngineInformation(voe::Statistics& engineStatistics)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::SetEngineInformation()");
    _engineStatisticsPtr = &engineStatistics;
    return 0;
}

int32_t
OutputMixer::SetAudioProcessingModule(AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "OutputMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)", audioProcessingModule);
    _audioProcessingModulePtr = audioProcessingModule;
    return 0;
}

int32_t
OutputMixer::SetMixabilityStatus(MixerParticipant& participant,
                                 bool mixable)
{
    return _mixerModule.SetMixabilityStatus(&participant, mixable);
}

int32_t
OutputMixer::MixActiveChannels()
{
    _mixerModule.Process();
    return 0;
}

int OutputMixer::GetMixedAudio(int sample_rate_hz,
                               size_t num_channels,
                               AudioFrame* frame) {
  WEBRTC_TRACE(
      kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
      "OutputMixer::GetMixedAudio(sample_rate_hz=%d, num_channels=%" PRIuS ")",
      sample_rate_hz, num_channels);

  frame->num_channels_ = num_channels;
  frame->sample_rate_hz_ = sample_rate_hz;
  // TODO(andrew): Ideally the downmixing would occur much earlier, in
  // AudioCodingModule.
  RemixAndResample(_audioFrame, &resampler_, frame);
  return 0;
}

int32_t
OutputMixer::DoOperationsOnCombinedSignal(bool feed_data_to_apm)
{
    if (_audioFrame.sample_rate_hz_ != _mixingFrequencyHz)
    {
        WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                     "OutputMixer::DoOperationsOnCombinedSignal() => "
                     "mixing frequency = %d", _audioFrame.sample_rate_hz_);
        _mixingFrequencyHz = _audioFrame.sample_rate_hz_;
    }

    // --- Far-end Voice Quality Enhancement (AudioProcessing Module)
    if (feed_data_to_apm) {
      if (_audioProcessingModulePtr->ProcessReverseStream(&_audioFrame) != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "AudioProcessingModule::ProcessReverseStream() => error");
        RTC_NOTREACHED();
      }
    }

    return 0;
}
}  // namespace voe
}  // namespace webrtc
