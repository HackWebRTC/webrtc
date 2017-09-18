/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/transmit_mixer.h"

#include <memory>

#include "audio/utility/audio_frame_operations.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/event_wrapper.h"
#include "system_wrappers/include/trace.h"
#include "voice_engine/channel.h"
#include "voice_engine/channel_manager.h"
#include "voice_engine/statistics.h"
#include "voice_engine/utility.h"
#include "voice_engine/voe_base_impl.h"

namespace webrtc {
namespace voe {

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
// TODO(ajm): The thread safety of this is dubious...
void TransmitMixer::OnPeriodicProcess()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::OnPeriodicProcess()");

    bool send_typing_noise_warning = false;
    bool typing_noise_detected = false;
    {
      rtc::CritScope cs(&_critSect);
      if (_typingNoiseWarningPending) {
        send_typing_noise_warning = true;
        typing_noise_detected = _typingNoiseDetected;
        _typingNoiseWarningPending = false;
      }
    }
    if (send_typing_noise_warning) {
        rtc::CritScope cs(&_callbackCritSect);
        if (_voiceEngineObserverPtr) {
            if (typing_noise_detected) {
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "TransmitMixer::OnPeriodicProcess() => "
                             "CallbackOnError(VE_TYPING_NOISE_WARNING)");
                _voiceEngineObserverPtr->CallbackOnError(
                    -1,
                    VE_TYPING_NOISE_WARNING);
            } else {
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "TransmitMixer::OnPeriodicProcess() => "
                             "CallbackOnError(VE_TYPING_NOISE_OFF_WARNING)");
                _voiceEngineObserverPtr->CallbackOnError(
                    -1,
                    VE_TYPING_NOISE_OFF_WARNING);
            }
        }
    }
}
#endif  // WEBRTC_VOICE_ENGINE_TYPING_DETECTION

int32_t
TransmitMixer::Create(TransmitMixer*& mixer, uint32_t instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                 "TransmitMixer::Create(instanceId=%d)", instanceId);
    mixer = new TransmitMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                     "TransmitMixer::Create() unable to allocate memory"
                     "for mixer");
        return -1;
    }
    return 0;
}

void
TransmitMixer::Destroy(TransmitMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

TransmitMixer::TransmitMixer(uint32_t instanceId) :
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _monitorModule(this),
#endif
    _instanceId(instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::TransmitMixer() - ctor");
}

TransmitMixer::~TransmitMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::~TransmitMixer() - dtor");
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    if (_processThreadPtr)
        _processThreadPtr->DeRegisterModule(&_monitorModule);
#endif
}

int32_t
TransmitMixer::SetEngineInformation(ProcessThread& processThread,
                                    Statistics& engineStatistics,
                                    ChannelManager& channelManager)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetEngineInformation()");

    _processThreadPtr = &processThread;
    _engineStatisticsPtr = &engineStatistics;
    _channelManagerPtr = &channelManager;

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _processThreadPtr->RegisterModule(&_monitorModule, RTC_FROM_HERE);
#endif
    return 0;
}

int32_t
TransmitMixer::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RegisterVoiceEngineObserver()");
    rtc::CritScope cs(&_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

int32_t
TransmitMixer::SetAudioProcessingModule(AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)",
                 audioProcessingModule);
    audioproc_ = audioProcessingModule;
    return 0;
}

void TransmitMixer::GetSendCodecInfo(int* max_sample_rate,
                                     size_t* max_channels) {
  *max_sample_rate = 8000;
  *max_channels = 1;
  for (ChannelManager::Iterator it(_channelManagerPtr); it.IsValid();
       it.Increment()) {
    Channel* channel = it.GetChannel();
    if (channel->Sending()) {
      CodecInst codec;
      // TODO(ossu): Investigate how this could happen. b/62909493
      if (channel->GetSendCodec(codec) == 0) {
        *max_sample_rate = std::max(*max_sample_rate, codec.plfreq);
        *max_channels = std::max(*max_channels, codec.channels);
      } else {
        LOG(LS_WARNING) << "Unable to get send codec for channel "
                        << channel->ChannelId();
        RTC_NOTREACHED();
      }
    }
  }
}

int32_t
TransmitMixer::PrepareDemux(const void* audioSamples,
                            size_t nSamples,
                            size_t nChannels,
                            uint32_t samplesPerSec,
                            uint16_t totalDelayMS,
                            int32_t clockDrift,
                            uint16_t currentMicLevel,
                            bool keyPressed)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PrepareDemux(nSamples=%" PRIuS ", "
                 "nChannels=%" PRIuS ", samplesPerSec=%u, totalDelayMS=%u, "
                 "clockDrift=%d, currentMicLevel=%u)",
                 nSamples, nChannels, samplesPerSec, totalDelayMS, clockDrift,
                 currentMicLevel);

    // --- Resample input audio and create/store the initial audio frame
    GenerateAudioFrame(static_cast<const int16_t*>(audioSamples),
                       nSamples,
                       nChannels,
                       samplesPerSec);

    // --- Near-end audio processing.
    ProcessAudio(totalDelayMS, clockDrift, currentMicLevel, keyPressed);

    if (swap_stereo_channels_ && stereo_codec_)
      // Only bother swapping if we're using a stereo codec.
      AudioFrameOperations::SwapStereoChannels(&_audioFrame);

    // --- Annoying typing detection (utilizes the APM/VAD decision)
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    TypingDetection(keyPressed);
#endif

    // --- Measure audio level of speech after all processing.
    double sample_duration = static_cast<double>(nSamples) / samplesPerSec;
    _audioLevel.ComputeLevel(_audioFrame, sample_duration);

    return 0;
}

void TransmitMixer::ProcessAndEncodeAudio() {
  RTC_DCHECK_GT(_audioFrame.samples_per_channel_, 0);
  for (ChannelManager::Iterator it(_channelManagerPtr); it.IsValid();
       it.Increment()) {
    Channel* const channel = it.GetChannel();
    if (channel->Sending()) {
      channel->ProcessAndEncodeAudio(_audioFrame);
    }
  }
}

uint32_t TransmitMixer::CaptureLevel() const
{
    return _captureLevel;
}

int32_t
TransmitMixer::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StopSend()");
    _audioLevel.Clear();
    return 0;
}

int8_t TransmitMixer::AudioLevel() const
{
    // Speech + file level [0,9]
    return _audioLevel.Level();
}

int16_t TransmitMixer::AudioLevelFullRange() const
{
    // Speech + file level [0,32767]
    return _audioLevel.LevelFullRange();
}

double TransmitMixer::GetTotalInputEnergy() const {
  return _audioLevel.TotalEnergy();
}

double TransmitMixer::GetTotalInputDuration() const {
  return _audioLevel.TotalDuration();
}

void TransmitMixer::GenerateAudioFrame(const int16_t* audio,
                                       size_t samples_per_channel,
                                       size_t num_channels,
                                       int sample_rate_hz) {
  int codec_rate;
  size_t num_codec_channels;
  GetSendCodecInfo(&codec_rate, &num_codec_channels);
  stereo_codec_ = num_codec_channels == 2;

  // We want to process at the lowest rate possible without losing information.
  // Choose the lowest native rate at least equal to the input and codec rates.
  const int min_processing_rate = std::min(sample_rate_hz, codec_rate);
  for (size_t i = 0; i < AudioProcessing::kNumNativeSampleRates; ++i) {
    _audioFrame.sample_rate_hz_ = AudioProcessing::kNativeSampleRatesHz[i];
    if (_audioFrame.sample_rate_hz_ >= min_processing_rate) {
      break;
    }
  }
  _audioFrame.num_channels_ = std::min(num_channels, num_codec_channels);
  RemixAndResample(audio, samples_per_channel, num_channels, sample_rate_hz,
                   &resampler_, &_audioFrame);
}

void TransmitMixer::ProcessAudio(int delay_ms, int clock_drift,
                                 int current_mic_level, bool key_pressed) {
  if (audioproc_->set_stream_delay_ms(delay_ms) != 0) {
    // Silently ignore this failure to avoid flooding the logs.
  }

  GainControl* agc = audioproc_->gain_control();
  if (agc->set_stream_analog_level(current_mic_level) != 0) {
    LOG(LS_ERROR) << "set_stream_analog_level failed: current_mic_level = "
                  << current_mic_level;
    assert(false);
  }

  EchoCancellation* aec = audioproc_->echo_cancellation();
  if (aec->is_drift_compensation_enabled()) {
    aec->set_stream_drift_samples(clock_drift);
  }

  audioproc_->set_stream_key_pressed(key_pressed);

  int err = audioproc_->ProcessStream(&_audioFrame);
  if (err != 0) {
    LOG(LS_ERROR) << "ProcessStream() error: " << err;
    assert(false);
  }

  // Store new capture level. Only updated when analog AGC is enabled.
  _captureLevel = agc->stream_analog_level();
}

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
void TransmitMixer::TypingDetection(bool keyPressed)
{
  // We let the VAD determine if we're using this feature or not.
  if (_audioFrame.vad_activity_ == AudioFrame::kVadUnknown) {
    return;
  }

  bool vadActive = _audioFrame.vad_activity_ == AudioFrame::kVadActive;
  if (_typingDetection.Process(keyPressed, vadActive)) {
    rtc::CritScope cs(&_critSect);
    _typingNoiseWarningPending = true;
    _typingNoiseDetected = true;
  } else {
    rtc::CritScope cs(&_critSect);
    // If there is already a warning pending, do not change the state.
    // Otherwise set a warning pending if last callback was for noise detected.
    if (!_typingNoiseWarningPending && _typingNoiseDetected) {
      _typingNoiseWarningPending = true;
      _typingNoiseDetected = false;
    }
  }
}
#endif

void TransmitMixer::EnableStereoChannelSwapping(bool enable) {
  swap_stereo_channels_ = enable;
}

bool TransmitMixer::IsStereoChannelSwappingEnabled() {
  return swap_stereo_channels_;
}

}  // namespace voe
}  // namespace webrtc
