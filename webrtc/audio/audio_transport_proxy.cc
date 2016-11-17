/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_transport_proxy.h"

namespace webrtc {

AudioTransportProxy::AudioTransportProxy(AudioTransport* voe_audio_transport,
                                         AudioProcessing* apm,
                                         AudioMixer* mixer)
    : voe_audio_transport_(voe_audio_transport) {
  RTC_DCHECK(voe_audio_transport);
  RTC_DCHECK(apm);
}

AudioTransportProxy::~AudioTransportProxy() {}

int32_t AudioTransportProxy::RecordedDataIsAvailable(
    const void* audioSamples,
    const size_t nSamples,
    const size_t nBytesPerSample,
    const size_t nChannels,
    const uint32_t samplesPerSec,
    const uint32_t totalDelayMS,
    const int32_t clockDrift,
    const uint32_t currentMicLevel,
    const bool keyPressed,
    uint32_t& newMicLevel) {
  // Pass call through to original audio transport instance.
  return voe_audio_transport_->RecordedDataIsAvailable(
      audioSamples, nSamples, nBytesPerSample, nChannels, samplesPerSec,
      totalDelayMS, clockDrift, currentMicLevel, keyPressed, newMicLevel);
}

int32_t AudioTransportProxy::NeedMorePlayData(const size_t nSamples,
                                              const size_t nBytesPerSample,
                                              const size_t nChannels,
                                              const uint32_t samplesPerSec,
                                              void* audioSamples,
                                              size_t& nSamplesOut,
                                              int64_t* elapsed_time_ms,
                                              int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(sizeof(int16_t) * nChannels, nBytesPerSample);
  RTC_DCHECK_GE(nChannels, 1u);
  RTC_DCHECK_LE(nChannels, 2u);
  RTC_DCHECK_GE(
      samplesPerSec,
      static_cast<uint32_t>(AudioProcessing::NativeRate::kSampleRate8kHz));
  RTC_DCHECK_EQ(nSamples * 100, samplesPerSec);
  RTC_DCHECK_LE(nBytesPerSample * nSamples * nChannels,
                sizeof(AudioFrame::data_));

  // Pass call through to original audio transport instance.
  return voe_audio_transport_->NeedMorePlayData(
      nSamples, nBytesPerSample, nChannels, samplesPerSec, audioSamples,
      nSamplesOut, elapsed_time_ms, ntp_time_ms);
}

void AudioTransportProxy::PushCaptureData(int voe_channel,
                                          const void* audio_data,
                                          int bits_per_sample,
                                          int sample_rate,
                                          size_t number_of_channels,
                                          size_t number_of_frames) {
  // This is part of deprecated VoE interface operating on specific
  // VoE channels. It should not be used.
  RTC_NOTREACHED();
}

void AudioTransportProxy::PullRenderData(int bits_per_sample,
                                         int sample_rate,
                                         size_t number_of_channels,
                                         size_t number_of_frames,
                                         void* audio_data,
                                         int64_t* elapsed_time_ms,
                                         int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(static_cast<size_t>(bits_per_sample), 8 * sizeof(int16_t));
  RTC_DCHECK_GE(number_of_channels, 1u);
  RTC_DCHECK_LE(number_of_channels, 2u);
  RTC_DCHECK_GE(static_cast<int>(sample_rate),
                AudioProcessing::NativeRate::kSampleRate8kHz);
  RTC_DCHECK_EQ(static_cast<int>(number_of_frames * 100), sample_rate);
  RTC_DCHECK_LE(bits_per_sample / 8 * number_of_frames * number_of_channels,
                sizeof(AudioFrame::data_));
  voe_audio_transport_->PullRenderData(
      bits_per_sample, sample_rate, number_of_channels, number_of_frames,
      audio_data, elapsed_time_ms, ntp_time_ms);
}

}  // namespace webrtc
