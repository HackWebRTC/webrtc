/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "echo_control_mobile_impl.h"

#include <cassert>

#include "critical_section_wrapper.h"
#include "echo_control_mobile.h"

#include "audio_processing_impl.h"
#include "audio_buffer.h"

namespace webrtc {

typedef void Handle;

EchoControlMobileImpl::EchoControlMobileImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm),
    routing_mode_(kSpeakerphone),
    comfort_noise_enabled_(true) {}

EchoControlMobileImpl::~EchoControlMobileImpl() {}

int EchoControlMobileImpl::ProcessRenderAudio(const AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->samples_per_split_channel() <= 160);
  assert(audio->num_channels() == apm_->num_reverse_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (int i = 0; i < apm_->num_output_channels(); i++) {
    for (int j = 0; j < audio->num_channels(); j++) {
      err = WebRtcAecm_BufferFarend(
          static_cast<Handle*>(handle(handle_index)),
          audio->low_pass_split_data(j),
          static_cast<WebRtc_Word16>(audio->samples_per_split_channel()));

      if (err != apm_->kNoError) {
        return TranslateError(err);  // TODO(ajm): warning possible?
      }

      handle_index++;
    }
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (!apm_->was_stream_delay_set()) {
    return apm_->kStreamParameterNotSetError;
  }

  assert(audio->samples_per_split_channel() <= 160);
  assert(audio->num_channels() == apm_->num_output_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AECM.
  size_t handle_index = 0;
  for (int i = 0; i < audio->num_channels(); i++) {
    // TODO(ajm): improve how this works, possibly inside AECM.
    //            This is kind of hacked up.
    WebRtc_Word16* noisy = audio->low_pass_reference(i);
    WebRtc_Word16* clean = audio->low_pass_split_data(i);
    if (noisy == NULL) {
      noisy = clean;
      clean = NULL;
    }
    for (int j = 0; j < apm_->num_reverse_channels(); j++) {
      err = WebRtcAecm_Process(
          static_cast<Handle*>(handle(handle_index)),
          noisy,
          clean,
          audio->low_pass_split_data(i),
          static_cast<WebRtc_Word16>(audio->samples_per_split_channel()),
          apm_->stream_delay_ms());

      if (err != apm_->kNoError) {
        return TranslateError(err);  // TODO(ajm): warning possible?
      }

      handle_index++;
    }
  }

  return apm_->kNoError;
}

int EchoControlMobileImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  // Ensure AEC and AECM are not both enabled.
  if (enable && apm_->echo_cancellation()->is_enabled()) {
    return apm_->kBadParameterError;
  }

  return EnableComponent(enable);
}

bool EchoControlMobileImpl::is_enabled() const {
  return is_component_enabled();
}

int EchoControlMobileImpl::set_routing_mode(RoutingMode mode) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  if (mode != kQuietEarpieceOrHeadset &&
      mode != kEarpiece &&
      mode != kLoudEarpiece &&
      mode != kSpeakerphone &&
      mode != kLoudSpeakerphone) {
    return apm_->kBadParameterError;
  }

  routing_mode_ = mode;
  return Configure();
}

EchoControlMobile::RoutingMode EchoControlMobileImpl::routing_mode()
    const {
  return routing_mode_;
}

int EchoControlMobileImpl::enable_comfort_noise(bool enable) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  comfort_noise_enabled_ = enable;
  return Configure();
}

bool EchoControlMobileImpl::is_comfort_noise_enabled() const {
  return comfort_noise_enabled_;
}

int EchoControlMobileImpl::Initialize() {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (apm_->sample_rate_hz() == apm_->kSampleRate32kHz) {
    // AECM doesn't support super-wideband.
    return apm_->kBadSampleRateError;
  }

  return ProcessingComponent::Initialize();
}

int EchoControlMobileImpl::get_version(char* version,
                                       int version_len_bytes) const {
  if (WebRtcAecm_get_version(version, version_len_bytes) != 0) {
      return apm_->kBadParameterError;
  }

  return apm_->kNoError;
}

void* EchoControlMobileImpl::CreateHandle() const {
  Handle* handle = NULL;
  if (WebRtcAecm_Create(&handle) != apm_->kNoError) {
    handle = NULL;
  } else {
    assert(handle != NULL);
  }

  return handle;
}

int EchoControlMobileImpl::DestroyHandle(void* handle) const {
  return WebRtcAecm_Free(static_cast<Handle*>(handle));
}

int EchoControlMobileImpl::InitializeHandle(void* handle) const {
  return WebRtcAecm_Init(static_cast<Handle*>(handle),
                         apm_->sample_rate_hz(),
                         48000); // Dummy value. This isn't actually
                                 // required by AECM.
}

/*int EchoControlMobileImpl::InitializeHandles(
    const vector<void*>& handles) const {
  int err = apm_->kNoError;

  for (size_t i = 0; i < num_handles(); i++) {
    err = WebRtcAec_Init(static_cast<Handle*>(handles[i]),
                        apm_->SampleRateHz(),
                        device_sample_rate_hz_);
    if (err != apm_->kNoError) {
      return TranslateError(err);
    }
  }

  return apm_->kNoError;
}*/

int EchoControlMobileImpl::ConfigureHandle(void* handle) const {
  AecmConfig config;
  config.cngMode = comfort_noise_enabled_;
  config.echoMode = routing_mode_;

  return WebRtcAecm_set_config(static_cast<Handle*>(handle), config);
}

int EchoControlMobileImpl::num_handles_required() const {
  return apm_->num_output_channels() *
         apm_->num_reverse_channels();
}

int EchoControlMobileImpl::TranslateError(int err) const {
  if (err == AECM_UNSUPPORTED_FUNCTION_ERROR) {
    return apm_->kUnsupportedFunctionError;
  } else if (err == AECM_BAD_PARAMETER_ERROR) {
    return apm_->kBadParameterError;
  } else if (err == AECM_BAD_PARAMETER_WARNING) {
    return apm_->kBadStreamParameterWarning;
  } else {
    // AECMOBFIX_UNSPECIFIED_ERROR
    // AECMOBFIX_UNINITIALIZED_ERROR
    // AECMOBFIX_NULL_POINTER_ERROR
    return apm_->kUnspecifiedError;
  }
}
}  // namespace webrtc

/*int EchoControlMobileImpl::GetConfiguration(void* handle) {
  if (!initialized_) {
    return apm_->kNoError;
  }

  AecConfig config;
  int err = WebRtcAec_get_config(handle, &config);
  if (err != apm_->kNoError) {
    return TranslateError(err);
  }

  if (config.metricsMode == 0) {
    metrics_enabled_ = false;
  } else if (config.metricsMode == 1) {
    metrics_enabled_ = true;
  } else {
    return apm_->kUnspecifiedError;
  }

  if (config.nlpMode == kAecNlpConservative) {
    routing_mode_ = kLowSuppression;
  } else if (config.nlpMode == kAecNlpModerate) {
    routing_mode_ = kMediumSuppression;
  } else if (config.nlpMode == kAecNlpAggressive) {
    routing_mode_ = kHighSuppression;
  } else {
    return apm_->kUnspecifiedError;
  }

  if (config.skewMode == 0) {
    drift_compensation_enabled_ = false;
  } else if (config.skewMode == 1) {
    drift_compensation_enabled_ = true;
  } else {
    return apm_->kUnspecifiedError;
  }

  return apm_->kNoError;
}*/
