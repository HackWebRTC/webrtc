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

namespace {
WebRtc_Word16 MapSetting(EchoControlMobile::RoutingMode mode) {
  switch (mode) {
    case EchoControlMobile::kQuietEarpieceOrHeadset:
      return 0;
    case EchoControlMobile::kEarpiece:
      return 1;
    case EchoControlMobile::kLoudEarpiece:
      return 2;
    case EchoControlMobile::kSpeakerphone:
      return 3;
    case EchoControlMobile::kLoudSpeakerphone:
      return 4;
    default:
      return -1;
  }
}

int MapError(int err) {
  switch (err) {
    case AECM_UNSUPPORTED_FUNCTION_ERROR:
      return AudioProcessing::kUnsupportedFunctionError;
    case AECM_BAD_PARAMETER_ERROR:
      return AudioProcessing::kBadParameterError;
    case AECM_BAD_PARAMETER_WARNING:
      return AudioProcessing::kBadStreamParameterWarning;
    default:
      // AECMOBFIX_UNSPECIFIED_ERROR
      // AECMOBFIX_UNINITIALIZED_ERROR
      // AECMOBFIX_NULL_POINTER_ERROR
      return AudioProcessing::kUnspecifiedError;
  }
}
}  // namespace

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
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_BufferFarend(
          my_handle,
          audio->low_pass_split_data(j),
          static_cast<WebRtc_Word16>(audio->samples_per_split_channel()));

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);  // TODO(ajm): warning possible?
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
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAecm_Process(
          my_handle,
          noisy,
          clean,
          audio->low_pass_split_data(i),
          static_cast<WebRtc_Word16>(audio->samples_per_split_channel()),
          apm_->stream_delay_ms());

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);  // TODO(ajm): warning possible?
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
  if (MapSetting(mode) == -1) {
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

int EchoControlMobileImpl::ConfigureHandle(void* handle) const {
  AecmConfig config;
  config.cngMode = comfort_noise_enabled_;
  config.echoMode = MapSetting(routing_mode_);

  return WebRtcAecm_set_config(static_cast<Handle*>(handle), config);
}

int EchoControlMobileImpl::num_handles_required() const {
  return apm_->num_output_channels() *
         apm_->num_reverse_channels();
}

int EchoControlMobileImpl::GetHandleError(void* handle) const {
  assert(handle != NULL);
  return MapError(WebRtcAecm_get_error_code(static_cast<Handle*>(handle)));
}
}  // namespace webrtc
