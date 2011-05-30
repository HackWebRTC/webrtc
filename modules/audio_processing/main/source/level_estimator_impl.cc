/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "level_estimator_impl.h"

#include <cassert>
#include <cstring>

#include "critical_section_wrapper.h"

#include "audio_processing_impl.h"
#include "audio_buffer.h"

// TODO(ajm): implement the underlying level estimator component.

namespace webrtc {

typedef void Handle;

namespace {

/*int EstimateLevel(AudioBuffer* audio, Handle* my_handle) {
  assert(audio->samples_per_split_channel() <= 160);

  WebRtc_Word16* mixed_data = audio->low_pass_split_data(0);
  if (audio->num_channels() > 1) {
    audio->CopyAndMixLowPass(1);
    mixed_data = audio->mixed_low_pass_data(0);
  }

  int err = UpdateLvlEst(my_handle,
                         mixed_data,
                         audio->samples_per_split_channel());
  if (err != AudioProcessing::kNoError) {
    return TranslateError(err);
  }

  return AudioProcessing::kNoError;
}

int GetMetricsLocal(Handle* my_handle, LevelEstimator::Metrics* metrics) {
  level_t levels;
  memset(&levels, 0, sizeof(levels));

  int err = ExportLevels(my_handle, &levels, 2);
  if (err != AudioProcessing::kNoError) {
    return err;
  }
  metrics->signal.instant = levels.instant;
  metrics->signal.average = levels.average;
  metrics->signal.maximum = levels.max;
  metrics->signal.minimum = levels.min;

  err = ExportLevels(my_handle, &levels, 1);
  if (err != AudioProcessing::kNoError) {
    return err;
  }
  metrics->speech.instant = levels.instant;
  metrics->speech.average = levels.average;
  metrics->speech.maximum = levels.max;
  metrics->speech.minimum = levels.min;

  err = ExportLevels(my_handle, &levels, 0);
  if (err != AudioProcessing::kNoError) {
    return err;
  }
  metrics->noise.instant = levels.instant;
  metrics->noise.average = levels.average;
  metrics->noise.maximum = levels.max;
  metrics->noise.minimum = levels.min;

  return AudioProcessing::kNoError;
}*/
}  // namespace

LevelEstimatorImpl::LevelEstimatorImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm) {}

LevelEstimatorImpl::~LevelEstimatorImpl() {}

int LevelEstimatorImpl::AnalyzeReverseStream(AudioBuffer* audio) {
  return apm_->kUnsupportedComponentError;
  /*if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  return EstimateLevel(audio, static_cast<Handle*>(handle(1)));*/
}

int LevelEstimatorImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  return apm_->kUnsupportedComponentError;
  /*if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  return EstimateLevel(audio, static_cast<Handle*>(handle(0)));*/
}

int LevelEstimatorImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(*apm_->crit());
  return apm_->kUnsupportedComponentError;
  //return EnableComponent(enable);
}

bool LevelEstimatorImpl::is_enabled() const {
  return is_component_enabled();
}

int LevelEstimatorImpl::GetMetrics(LevelEstimator::Metrics* metrics,
                                   LevelEstimator::Metrics* reverse_metrics) {
  return apm_->kUnsupportedComponentError;
  /*if (!is_component_enabled()) {
    return apm_->kNotEnabledError;
  }

  int err = GetMetricsLocal(static_cast<Handle*>(handle(0)), metrics);
  if (err != apm_->kNoError) {
    return err;
  }

  err = GetMetricsLocal(static_cast<Handle*>(handle(1)), reverse_metrics);
  if (err != apm_->kNoError) {
    return err;
  }

  return apm_->kNoError;*/
}

int LevelEstimatorImpl::get_version(char* version,
                                    int version_len_bytes) const {
  // An empty string is used to indicate no version information.
  memset(version, 0, version_len_bytes);
  return apm_->kNoError;
}

void* LevelEstimatorImpl::CreateHandle() const {
  Handle* handle = NULL;
  /*if (CreateLvlEst(&handle) != apm_->kNoError) {
    handle = NULL;
  } else {
    assert(handle != NULL);
  }*/

  return handle;
}

int LevelEstimatorImpl::DestroyHandle(void* handle) const {
  return apm_->kUnsupportedComponentError;
  //return FreeLvlEst(static_cast<Handle*>(handle));
}

int LevelEstimatorImpl::InitializeHandle(void* handle) const {
  return apm_->kUnsupportedComponentError;
  /*const double kIntervalSeconds = 1.5;
  return InitLvlEst(static_cast<Handle*>(handle),
                    apm_->sample_rate_hz(),
                    kIntervalSeconds);*/
}

/*int LevelEstimatorImpl::InitializeHandles(
    const vector<void*>& handles) const {
  int err = apm_->kNoError;

  for (size_t i = 0; i < num_handles(); i++) {
    err = InitLvlEst(static_cast<Handle*>(handles[i]), apm_->SampleRateHz());
    if (err != apm_->kNoError) {
      return TranslateError(err);
    }
  }

  return apm_->kNoError;
}*/

int LevelEstimatorImpl::ConfigureHandle(void* /*handle*/) const {
  return apm_->kUnsupportedComponentError;
  //return apm_->kNoError;
}

int LevelEstimatorImpl::num_handles_required() const {
  return apm_->kUnsupportedComponentError;
  //return 2;
}

//int LevelEstimatorImpl::GetConfiguration() {
//  // There are no configuration accessors.
//  return apm_->kUnsupportedFunctionError;
//}

}  // namespace webrtc
