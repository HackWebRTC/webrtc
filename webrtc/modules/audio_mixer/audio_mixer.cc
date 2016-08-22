/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/audio_mixer.h"

#include "webrtc/base/format_macros.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/statistics.h"
#include "webrtc/voice_engine/utility.h"

namespace webrtc {
namespace voe {

void AudioMixer::PlayNotification(int32_t id, uint32_t durationMs) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::PlayNotification(id=%d, durationMs=%d)", id,
               durationMs);
  // Not implement yet
}

void AudioMixer::RecordNotification(int32_t id, uint32_t durationMs) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::RecordNotification(id=%d, durationMs=%d)", id,
               durationMs);

  // Not implement yet
}

void AudioMixer::PlayFileEnded(int32_t id) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::PlayFileEnded(id=%d)", id);

  // not needed
}

void AudioMixer::RecordFileEnded(int32_t id) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::RecordFileEnded(id=%d)", id);
  RTC_DCHECK_EQ(id, _instanceId);

  rtc::CritScope cs(&_fileCritSect);
  _outputFileRecording = false;
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::RecordFileEnded() =>"
               "output file recorder module is shutdown");
}

int32_t AudioMixer::Create(AudioMixer*& mixer, uint32_t instanceId) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
               "AudioMixer::Create(instanceId=%d)", instanceId);
  mixer = new AudioMixer(instanceId);
  if (mixer == NULL) {
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, instanceId,
                 "AudioMixer::Create() unable to allocate memory for"
                 "mixer");
    return -1;
  }
  return 0;
}

AudioMixer::AudioMixer(uint32_t instanceId)
    : _mixerModule(*NewAudioConferenceMixer::Create(instanceId)),
      _audioLevel(),
      _instanceId(instanceId),
      _externalMediaCallbackPtr(NULL),
      _externalMedia(false),
      _panLeft(1.0f),
      _panRight(1.0f),
      _mixingFrequencyHz(8000),
      _outputFileRecording(false) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::AudioMixer() - ctor");
}

void AudioMixer::Destroy(AudioMixer*& mixer) {
  if (mixer) {
    delete mixer;
    mixer = NULL;
  }
}

AudioMixer::~AudioMixer() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::~AudioMixer() - dtor");
  if (_externalMedia) {
    DeRegisterExternalMediaProcessing();
  }
  {
    rtc::CritScope cs(&_fileCritSect);
    if (_outputFileRecorderPtr) {
      _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
      _outputFileRecorderPtr->StopRecording();
    }
  }
  delete &_mixerModule;
}

int32_t AudioMixer::SetEngineInformation(voe::Statistics& engineStatistics) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::SetEngineInformation()");
  _engineStatisticsPtr = &engineStatistics;
  return 0;
}

int32_t AudioMixer::SetAudioProcessingModule(
    AudioProcessing* audioProcessingModule) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::SetAudioProcessingModule("
               "audioProcessingModule=0x%x)",
               audioProcessingModule);
  _audioProcessingModulePtr = audioProcessingModule;
  return 0;
}

int AudioMixer::RegisterExternalMediaProcessing(
    VoEMediaProcess& proccess_object) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::RegisterExternalMediaProcessing()");

  rtc::CritScope cs(&_callbackCritSect);
  _externalMediaCallbackPtr = &proccess_object;
  _externalMedia = true;

  return 0;
}

int AudioMixer::DeRegisterExternalMediaProcessing() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::DeRegisterExternalMediaProcessing()");

  rtc::CritScope cs(&_callbackCritSect);
  _externalMedia = false;
  _externalMediaCallbackPtr = NULL;

  return 0;
}

int32_t AudioMixer::SetMixabilityStatus(MixerAudioSource& audio_source,
                                        bool mixable) {
  return _mixerModule.SetMixabilityStatus(&audio_source, mixable);
}

int32_t AudioMixer::SetAnonymousMixabilityStatus(MixerAudioSource& audio_source,
                                                 bool mixable) {
  return _mixerModule.SetAnonymousMixabilityStatus(&audio_source, mixable);
}

int AudioMixer::GetSpeechOutputLevel(uint32_t& level) {
  int8_t currentLevel = _audioLevel.Level();
  level = static_cast<uint32_t>(currentLevel);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "GetSpeechOutputLevel() => level=%u", level);
  return 0;
}

int AudioMixer::GetSpeechOutputLevelFullRange(uint32_t& level) {
  int16_t currentLevel = _audioLevel.LevelFullRange();
  level = static_cast<uint32_t>(currentLevel);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "GetSpeechOutputLevelFullRange() => level=%u", level);
  return 0;
}

int AudioMixer::SetOutputVolumePan(float left, float right) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::SetOutputVolumePan()");
  _panLeft = left;
  _panRight = right;
  return 0;
}

int AudioMixer::GetOutputVolumePan(float& left, float& right) {
  left = _panLeft;
  right = _panRight;
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "GetOutputVolumePan() => left=%2.1f, right=%2.1f", left, right);
  return 0;
}

int AudioMixer::StartRecordingPlayout(const char* fileName,
                                      const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::StartRecordingPlayout(fileName=%s)", fileName);

  if (_outputFileRecording) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                 "StartRecordingPlayout() is already recording");
    return 0;
  }

  FileFormats format;
  const uint32_t notificationTime(0);
  CodecInst dummyCodec = {100, "L16", 16000, 320, 1, 320000};

  if ((codecInst != NULL) &&
      ((codecInst->channels < 1) || (codecInst->channels > 2))) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_ARGUMENT, kTraceError,
        "StartRecordingPlayout() invalid compression");
    return (-1);
  }
  if (codecInst == NULL) {
    format = kFileFormatPcm16kHzFile;
    codecInst = &dummyCodec;
  } else if ((STR_CASE_CMP(codecInst->plname, "L16") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMU") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMA") == 0)) {
    format = kFileFormatWavFile;
  } else {
    format = kFileFormatCompressedFile;
  }

  rtc::CritScope cs(&_fileCritSect);

  if (_outputFileRecorderPtr) {
    _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
  }

  _outputFileRecorderPtr =
      FileRecorder::CreateFileRecorder(_instanceId, (const FileFormats)format);
  if (_outputFileRecorderPtr == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartRecordingPlayout() fileRecorder format isnot correct");
    return -1;
  }

  if (_outputFileRecorderPtr->StartRecordingAudioFile(
          fileName, (const CodecInst&)*codecInst, notificationTime) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartRecordingAudioFile() failed to start file recording");
    _outputFileRecorderPtr->StopRecording();
    _outputFileRecorderPtr.reset();
    return -1;
  }
  _outputFileRecorderPtr->RegisterModuleFileCallback(this);
  _outputFileRecording = true;

  return 0;
}

int AudioMixer::StartRecordingPlayout(OutStream* stream,
                                      const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::StartRecordingPlayout()");

  if (_outputFileRecording) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                 "StartRecordingPlayout() is already recording");
    return 0;
  }

  FileFormats format;
  const uint32_t notificationTime(0);
  CodecInst dummyCodec = {100, "L16", 16000, 320, 1, 320000};

  if (codecInst != NULL && codecInst->channels != 1) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_ARGUMENT, kTraceError,
        "StartRecordingPlayout() invalid compression");
    return (-1);
  }
  if (codecInst == NULL) {
    format = kFileFormatPcm16kHzFile;
    codecInst = &dummyCodec;
  } else if ((STR_CASE_CMP(codecInst->plname, "L16") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMU") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMA") == 0)) {
    format = kFileFormatWavFile;
  } else {
    format = kFileFormatCompressedFile;
  }

  rtc::CritScope cs(&_fileCritSect);

  if (_outputFileRecorderPtr) {
    _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
  }

  _outputFileRecorderPtr =
      FileRecorder::CreateFileRecorder(_instanceId, (const FileFormats)format);
  if (_outputFileRecorderPtr == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartRecordingPlayout() fileRecorder format isnot correct");
    return -1;
  }

  if (_outputFileRecorderPtr->StartRecordingAudioFile(stream, *codecInst,
                                                      notificationTime) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartRecordingAudioFile() failed to start file recording");
    _outputFileRecorderPtr->StopRecording();
    _outputFileRecorderPtr.reset();
    return -1;
  }

  _outputFileRecorderPtr->RegisterModuleFileCallback(this);
  _outputFileRecording = true;

  return 0;
}

int AudioMixer::StopRecordingPlayout() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "AudioMixer::StopRecordingPlayout()");

  if (!_outputFileRecording) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                 "StopRecordingPlayout() file isnot recording");
    return -1;
  }

  rtc::CritScope cs(&_fileCritSect);

  if (_outputFileRecorderPtr->StopRecording() != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_STOP_RECORDING_FAILED, kTraceError,
        "StopRecording(), could not stop recording");
    return -1;
  }
  _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
  _outputFileRecorderPtr.reset();
  _outputFileRecording = false;

  return 0;
}

int AudioMixer::GetMixedAudio(int sample_rate_hz,
                              size_t num_channels,
                              AudioFrame* frame) {
  WEBRTC_TRACE(
      kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
      "AudioMixer::GetMixedAudio(sample_rate_hz=%d, num_channels=%" PRIuS ")",
      sample_rate_hz, num_channels);

  // --- Record playout if enabled
  {
    rtc::CritScope cs(&_fileCritSect);
    if (_outputFileRecording && _outputFileRecorderPtr)
      _outputFileRecorderPtr->RecordAudioToFile(_audioFrame);
  }

  _mixerModule.Mix(sample_rate_hz, num_channels, frame);

  return 0;
}

int32_t AudioMixer::DoOperationsOnCombinedSignal(bool feed_data_to_apm) {
  if (_audioFrame.sample_rate_hz_ != _mixingFrequencyHz) {
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "AudioMixer::DoOperationsOnCombinedSignal() => "
                 "mixing frequency = %d",
                 _audioFrame.sample_rate_hz_);
    _mixingFrequencyHz = _audioFrame.sample_rate_hz_;
  }

  // Scale left and/or right channel(s) if balance is active
  if (_panLeft != 1.0 || _panRight != 1.0) {
    if (_audioFrame.num_channels_ == 1) {
      AudioFrameOperations::MonoToStereo(&_audioFrame);
    } else {
      // Pure stereo mode (we are receiving a stereo signal).
    }

    RTC_DCHECK_EQ(_audioFrame.num_channels_, static_cast<size_t>(2));
    AudioFrameOperations::Scale(_panLeft, _panRight, _audioFrame);
  }

  // --- Far-end Voice Quality Enhancement (AudioProcessing Module)
  if (feed_data_to_apm) {
    if (_audioProcessingModulePtr->ProcessReverseStream(&_audioFrame) != 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "AudioProcessingModule::ProcessReverseStream() => error");
      RTC_DCHECK(false);
    }
  }

  // --- External media processing
  {
    rtc::CritScope cs(&_callbackCritSect);
    if (_externalMedia) {
      const bool is_stereo = (_audioFrame.num_channels_ == 2);
      if (_externalMediaCallbackPtr) {
        _externalMediaCallbackPtr->Process(
            -1, kPlaybackAllChannelsMixed,
            reinterpret_cast<int16_t*>(_audioFrame.data_),
            _audioFrame.samples_per_channel_, _audioFrame.sample_rate_hz_,
            is_stereo);
      }
    }
  }

  // --- Measure audio level (0-9) for the combined signal
  _audioLevel.ComputeLevel(_audioFrame);

  return 0;
}
}  // namespace voe
}  // namespace webrtc
