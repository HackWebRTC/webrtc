/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "noise_suppression_impl.h"

#include <cassert>

#include "critical_section_wrapper.h"
#if defined(WEBRTC_NS_FLOAT)
#include "noise_suppression.h"
#elif defined(WEBRTC_NS_FIXED)
#include "noise_suppression_x.h"
#endif

#include "audio_processing_impl.h"
#include "audio_buffer.h"

namespace webrtc {

#if defined(WEBRTC_NS_FLOAT)
typedef NsHandle Handle;
#elif defined(WEBRTC_NS_FIXED)
typedef NsxHandle Handle;
#endif

NoiseSuppressionImpl::NoiseSuppressionImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm),
    level_(kModerate) {}

NoiseSuppressionImpl::~NoiseSuppressionImpl() {}

int NoiseSuppressionImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  int err = apm_->kNoError;

  if (!is_component_enabled()) {
    return apm_->kNoError;
  }
  assert(audio->samples_per_split_channel() <= 160);
  assert(audio->num_channels() == num_handles());

  for (int i = 0; i < num_handles(); i++) {
#if defined(WEBRTC_NS_FLOAT)
    err = WebRtcNs_Process(static_cast<Handle*>(handle(i)),
                           audio->low_pass_split_data(i),
                           audio->high_pass_split_data(i),
                           audio->low_pass_split_data(i),
                           audio->high_pass_split_data(i));
#elif defined(WEBRTC_NS_FIXED)
    err = WebRtcNsx_Process(static_cast<Handle*>(handle(i)),
                            audio->low_pass_split_data(i),
                            audio->high_pass_split_data(i),
                            audio->low_pass_split_data(i),
                            audio->high_pass_split_data(i));
#endif

    if (err != apm_->kNoError) {
      return TranslateError(err);
    }
  }

  return apm_->kNoError;
}

int NoiseSuppressionImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  return EnableComponent(enable);
}

bool NoiseSuppressionImpl::is_enabled() const {
  return is_component_enabled();
}

int NoiseSuppressionImpl::set_level(Level level) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  if (level != kLow &&
      level != kModerate &&
      level != kHigh &&
      level != kVeryHigh) {
    return apm_->kBadParameterError;
  }

  level_ = level;
  return Configure();
}

NoiseSuppression::Level NoiseSuppressionImpl::level() const {
  return level_;
}

int NoiseSuppressionImpl::get_version(char* version,
                                      int version_len_bytes) const {
#if defined(WEBRTC_NS_FLOAT)
  if (WebRtcNs_get_version(version, version_len_bytes) != 0)
#elif defined(WEBRTC_NS_FIXED)
  if (WebRtcNsx_get_version(version, version_len_bytes) != 0)
#endif
  {
      return apm_->kBadParameterError;
  }

  return apm_->kNoError;
}

void* NoiseSuppressionImpl::CreateHandle() const {
  Handle* handle = NULL;
#if defined(WEBRTC_NS_FLOAT)
  if (WebRtcNs_Create(&handle) != apm_->kNoError)
#elif defined(WEBRTC_NS_FIXED)
  if (WebRtcNsx_Create(&handle) != apm_->kNoError)
#endif
  {
    handle = NULL;
  } else {
    assert(handle != NULL);
  }

  return handle;
}

int NoiseSuppressionImpl::DestroyHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_Free(static_cast<Handle*>(handle));
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_Free(static_cast<Handle*>(handle));
#endif
}

int NoiseSuppressionImpl::InitializeHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_Init(static_cast<Handle*>(handle), apm_->sample_rate_hz());
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_Init(static_cast<Handle*>(handle), apm_->sample_rate_hz());
#endif
}

/*int NoiseSuppressionImpl::InitializeHandles(
    const vector<void*>& handles) const {
  int err = apm_->kNoError;

  for (size_t i = 0; i < num_handles(); i++) {
    err = WebRtcNs_Init(static_cast<Handle*>(handles[i]), apm_->SampleRateHz());
    if (err != apm_->kNoError) {
      return TranslateError(err);
    }
  }

  return apm_->kNoError;
}*/

int NoiseSuppressionImpl::ConfigureHandle(void* handle) const {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_set_policy(static_cast<Handle*>(handle), level_);
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_set_policy(static_cast<Handle*>(handle), level_);
#endif
}

int NoiseSuppressionImpl::num_handles_required() const {
  return apm_->num_output_channels();
}

//int NoiseSuppressionImpl::GetConfiguration() {
//  // There are no configuration accessors.
//  return apm_->kUnsupportedFunctionError;
//}

// TODO(ajm): implement
int NoiseSuppressionImpl::TranslateError(int /*err*/) const {
  return -1;
}
}  // namespace webrtc

