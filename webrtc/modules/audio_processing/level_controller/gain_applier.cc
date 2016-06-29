/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_controller/gain_applier.h"

#include <algorithm>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

const float kMaxSampleValue = 32767.f;
const float kMinSampleValue = -32767.f;

int CountSaturations(rtc::ArrayView<const float> in) {
  return std::count_if(in.begin(), in.end(), [](const float& v) {
    return v >= kMaxSampleValue || v <= kMinSampleValue;
  });
}

int CountSaturations(const AudioBuffer& audio) {
  int num_saturations = 0;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    num_saturations += CountSaturations(rtc::ArrayView<const float>(
        audio.channels_const_f()[k], audio.num_frames()));
  }
  return num_saturations;
}

void LimitToAllowedRange(rtc::ArrayView<float> x) {
  for (auto& v : x) {
    v = std::max(kMinSampleValue, v);
    v = std::min(kMaxSampleValue, v);
  }
}

void LimitToAllowedRange(AudioBuffer* audio) {
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    LimitToAllowedRange(
        rtc::ArrayView<float>(audio->channels_f()[k], audio->num_frames()));
  }
}

float ApplyIncreasingGain(float new_gain,
                          float old_gain,
                          float step_size,
                          rtc::ArrayView<float> x) {
  RTC_DCHECK_LT(0.f, step_size);
  float gain = old_gain;
  for (auto& v : x) {
    gain = std::min(new_gain, gain + step_size);
    v *= gain;
  }
  return gain;
}

float ApplyDecreasingGain(float new_gain,
                          float old_gain,
                          float step_size,
                          rtc::ArrayView<float> x) {
  RTC_DCHECK_LT(0.f, step_size);
  float gain = old_gain;
  for (auto& v : x) {
    gain = std::max(new_gain, gain - step_size);
    v *= gain;
  }
  return gain;
}

float ApplyConstantGain(float gain, rtc::ArrayView<float> x) {
  for (auto& v : x) {
    v *= gain;
  }

  return gain;
}

float ApplyGain(float new_gain,
                float old_gain,
                float step_size,
                rtc::ArrayView<float> x) {
  if (new_gain == old_gain) {
    return ApplyConstantGain(new_gain, x);
  } else if (new_gain > old_gain) {
    return ApplyIncreasingGain(new_gain, old_gain, step_size, x);
  } else {
    return ApplyDecreasingGain(new_gain, old_gain, step_size, x);
  }
}

}  // namespace

GainApplier::GainApplier(ApmDataDumper* data_dumper)
    : data_dumper_(data_dumper) {}

void GainApplier::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  const float kStepSize48kHz = 0.001f;
  old_gain_ = 1.f;
  gain_change_step_size_ =
      kStepSize48kHz *
      (static_cast<float>(AudioProcessing::kSampleRate48kHz) / sample_rate_hz);
}

int GainApplier::Process(float new_gain, AudioBuffer* audio) {
  RTC_CHECK_NE(0.f, gain_change_step_size_);
  int num_saturations = 0;
  if (new_gain != 1.f) {
    float last_applied_gain = 1.f;
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      // TODO(peah): Consider using a faster update rate downwards than upwards.
      last_applied_gain = ApplyGain(
          new_gain, old_gain_, gain_change_step_size_,
          rtc::ArrayView<float>(audio->channels_f()[k], audio->num_frames()));
    }
    // TODO(peah): Consider the need for faster gain reduction in case of
    // excessive saturation.
    num_saturations = CountSaturations(*audio);
    LimitToAllowedRange(audio);
    old_gain_ = last_applied_gain;
  }

  data_dumper_->DumpRaw("lc_last_applied_gain", 1, &old_gain_);

  return num_saturations;
}

}  // namespace webrtc
