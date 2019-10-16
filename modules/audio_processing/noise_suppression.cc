/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/noise_suppression.h"

#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"
#if defined(WEBRTC_NS_FLOAT)
#include "modules/audio_processing/ns/noise_suppression.h"

#define NS_CREATE WebRtcNs_Create
#define NS_FREE WebRtcNs_Free
#define NS_INIT WebRtcNs_Init
#define NS_SET_POLICY WebRtcNs_set_policy
typedef NsHandle NsState;
#elif defined(WEBRTC_NS_FIXED)
#include "modules/audio_processing/ns/noise_suppression_x.h"

#define NS_CREATE WebRtcNsx_Create
#define NS_FREE WebRtcNsx_Free
#define NS_INIT WebRtcNsx_Init
#define NS_SET_POLICY WebRtcNsx_set_policy
typedef NsxHandle NsState;
#endif

namespace webrtc {
namespace {
int NoiseSuppressionLevelToPolicy(NoiseSuppression::Level level) {
  switch (level) {
    case NoiseSuppression::Level::kLow:
      return 0;
    case NoiseSuppression::Level::kModerate:
      return 1;
    case NoiseSuppression::Level::kHigh:
      return 2;
    case NoiseSuppression::Level::kVeryHigh:
      return 3;
    default:
      RTC_NOTREACHED();
  }
  return 1;
}
}  // namespace

class NoiseSuppression::Suppressor {
 public:
  explicit Suppressor(int sample_rate_hz) {
    state_ = NS_CREATE();
    RTC_CHECK(state_);
    int error = NS_INIT(state_, sample_rate_hz);
    RTC_DCHECK_EQ(0, error);
  }
  ~Suppressor() { NS_FREE(state_); }

  Suppressor(Suppressor&) = delete;
  Suppressor& operator=(Suppressor&) = delete;

  NsState* state() { return state_; }

 private:
  NsState* state_ = nullptr;
};

NoiseSuppression::NoiseSuppression(size_t channels,
                                   int sample_rate_hz,
                                   Level level) {
  const int policy = NoiseSuppressionLevelToPolicy(level);
  for (size_t i = 0; i < channels; ++i) {
    suppressors_.push_back(std::make_unique<Suppressor>(sample_rate_hz));
    int error = NS_SET_POLICY(suppressors_[i]->state(), policy);
    RTC_DCHECK_EQ(0, error);
  }
}

NoiseSuppression::~NoiseSuppression() {}

void NoiseSuppression::AnalyzeCaptureAudio(AudioBuffer* audio) {
  RTC_DCHECK(audio);
#if defined(WEBRTC_NS_FLOAT)
  RTC_DCHECK_GE(160, audio->num_frames_per_band());
  RTC_DCHECK_EQ(suppressors_.size(), audio->num_channels());
  for (size_t i = 0; i < suppressors_.size(); i++) {
    WebRtcNs_Analyze(suppressors_[i]->state(),
                     audio->split_bands_const(i)[kBand0To8kHz]);
  }
#endif
}

void NoiseSuppression::ProcessCaptureAudio(AudioBuffer* audio) {
  RTC_DCHECK(audio);
  RTC_DCHECK_GE(160, audio->num_frames_per_band());
  RTC_DCHECK_EQ(suppressors_.size(), audio->num_channels());
  for (size_t i = 0; i < suppressors_.size(); i++) {
#if defined(WEBRTC_NS_FLOAT)
    WebRtcNs_Process(suppressors_[i]->state(), audio->split_bands_const(i),
                     audio->num_bands(), audio->split_bands(i));
#elif defined(WEBRTC_NS_FIXED)
    int16_t split_band_data[AudioBuffer::kMaxNumBands]
                           [AudioBuffer::kMaxSplitFrameLength];
    int16_t* split_bands[AudioBuffer::kMaxNumBands] = {
        split_band_data[0], split_band_data[1], split_band_data[2]};
    audio->ExportSplitChannelData(i, split_bands);

    WebRtcNsx_Process(suppressors_[i]->state(), split_bands, audio->num_bands(),
                      split_bands);

    audio->ImportSplitChannelData(i, split_bands);
#endif
  }
}

float NoiseSuppression::speech_probability() const {
#if defined(WEBRTC_NS_FLOAT)
  float probability_average = 0.0f;
  for (auto& suppressor : suppressors_) {
    probability_average +=
        WebRtcNs_prior_speech_probability(suppressor->state());
  }
  if (!suppressors_.empty()) {
    probability_average /= suppressors_.size();
  }
  return probability_average;
#elif defined(WEBRTC_NS_FIXED)
  // TODO(peah): Returning error code as a float! Remove this.
  // Currently not available for the fixed point implementation.
  return AudioProcessing::kUnsupportedFunctionError;
#endif
}

std::vector<float> NoiseSuppression::NoiseEstimate() {
  std::vector<float> noise_estimate;
#if defined(WEBRTC_NS_FLOAT)
  const float kNumChannelsFraction = 1.f / suppressors_.size();
  noise_estimate.assign(WebRtcNs_num_freq(), 0.f);
  for (auto& suppressor : suppressors_) {
    const float* noise = WebRtcNs_noise_estimate(suppressor->state());
    for (size_t i = 0; i < noise_estimate.size(); ++i) {
      noise_estimate[i] += kNumChannelsFraction * noise[i];
    }
  }
#elif defined(WEBRTC_NS_FIXED)
  noise_estimate.assign(WebRtcNsx_num_freq(), 0.f);
  for (auto& suppressor : suppressors_) {
    int q_noise;
    const uint32_t* noise =
        WebRtcNsx_noise_estimate(suppressor->state(), &q_noise);
    const float kNormalizationFactor =
        1.f / ((1 << q_noise) * suppressors_.size());
    for (size_t i = 0; i < noise_estimate.size(); ++i) {
      noise_estimate[i] += kNormalizationFactor * noise[i];
    }
  }
#endif
  return noise_estimate;
}

size_t NoiseSuppression::num_noise_bins() {
#if defined(WEBRTC_NS_FLOAT)
  return WebRtcNs_num_freq();
#elif defined(WEBRTC_NS_FIXED)
  return WebRtcNsx_num_freq();
#endif
}

}  // namespace webrtc
