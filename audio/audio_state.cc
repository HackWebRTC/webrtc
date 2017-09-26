/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_state.h"

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "voice_engine/transmit_mixer.h"

namespace webrtc {
namespace internal {

AudioState::AudioState(const AudioState::Config& config)
    : config_(config),
      voe_base_(config.voice_engine),
      audio_transport_proxy_(voe_base_->audio_transport(),
                             config_.audio_processing.get(),
                             config_.audio_mixer) {
  process_thread_checker_.DetachFromThread();
  RTC_DCHECK(config_.audio_mixer);

  auto* const device = voe_base_->audio_device_module();
  RTC_DCHECK(device);

  // This is needed for the Chrome implementation of RegisterAudioCallback.
  device->RegisterAudioCallback(nullptr);
  device->RegisterAudioCallback(&audio_transport_proxy_);
}

AudioState::~AudioState() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

VoiceEngine* AudioState::voice_engine() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.voice_engine;
}

rtc::scoped_refptr<AudioMixer> AudioState::mixer() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_.audio_mixer;
}

bool AudioState::typing_noise_detected() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(solenberg): Remove const_cast once AudioState owns transmit mixer
  //                  functionality.
  voe::TransmitMixer* transmit_mixer =
      const_cast<AudioState*>(this)->voe_base_->transmit_mixer();
  return transmit_mixer->typing_noise_detected();
}

// Reference count; implementation copied from rtc::RefCountedObject.
int AudioState::AddRef() const {
  return rtc::AtomicOps::Increment(&ref_count_);
}

// Reference count; implementation copied from rtc::RefCountedObject.
int AudioState::Release() const {
  int count = rtc::AtomicOps::Decrement(&ref_count_);
  if (!count) {
    delete this;
  }
  return count;
}
}  // namespace internal

rtc::scoped_refptr<AudioState> AudioState::Create(
    const AudioState::Config& config) {
  return rtc::scoped_refptr<AudioState>(new internal::AudioState(config));
}
}  // namespace webrtc
