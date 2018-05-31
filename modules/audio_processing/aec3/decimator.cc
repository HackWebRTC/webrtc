/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/decimator.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// b, a = signal.butter(2, 3400/8000.0, 'lowpass', analog=False) which are the
// same as b, a = signal.butter(2, 1700/4000.0, 'lowpass', analog=False).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients2 = {
    {0.22711796f, 0.45423593f, 0.22711796f},
    {-0.27666461f, 0.18513647f}};
constexpr int kNumFilters2 = 3;

// b, a = signal.butter(2, 750/8000.0, 'lowpass', analog=False) which are the
// same as b, a = signal.butter(2, 375/4000.0, 'lowpass', analog=False).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients4 = {
    {0.0179f, 0.0357f, 0.0179f},
    {-1.5879f, 0.6594f}};
constexpr int kNumFilters4 = 3;

// b, a = signal.cheby1(1, 6, [1000/8000, 2000/8000], btype='bandpass',
// analog=False)
const CascadedBiQuadFilter::BiQuadCoefficients kBandPassFilterCoefficients8 = {
    {0.10330478f, 0.f, -0.10330478f},
    {-1.520363f, 0.79339043f}};
constexpr int kNumFilters8 = 5;

// b, a = signal.butter(2, 1000/8000.0, 'highpass', analog=False)
const CascadedBiQuadFilter::BiQuadCoefficients kHighPassFilterCoefficients = {
    {0.75707638f, -1.51415275f, 0.75707638f},
    {-1.45424359f, 0.57406192f}};
constexpr int kNumFiltersHP2 = 1;
constexpr int kNumFiltersHP4 = 1;
constexpr int kNumFiltersHP8 = 0;

}  // namespace

Decimator::Decimator(size_t down_sampling_factor)
    : down_sampling_factor_(down_sampling_factor),
      anti_aliasing_filter_(
          down_sampling_factor_ == 4
              ? kLowPassFilterCoefficients4
              : (down_sampling_factor_ == 8 ? kBandPassFilterCoefficients8
                                            : kLowPassFilterCoefficients2),
          down_sampling_factor_ == 4
              ? kNumFilters4
              : (down_sampling_factor_ == 8 ? kNumFilters8 : kNumFilters2)),
      noise_reduction_filter_(
          kHighPassFilterCoefficients,
          down_sampling_factor_ == 4
              ? kNumFiltersHP4
              : (down_sampling_factor_ == 8 ? kNumFiltersHP8
                                            : kNumFiltersHP2)) {
  RTC_DCHECK(down_sampling_factor_ == 2 || down_sampling_factor_ == 4 ||
             down_sampling_factor_ == 8);
}

void Decimator::Decimate(rtc::ArrayView<const float> in,
                         rtc::ArrayView<float> out) {
  RTC_DCHECK_EQ(kBlockSize, in.size());
  RTC_DCHECK_EQ(kBlockSize / down_sampling_factor_, out.size());
  std::array<float, kBlockSize> x;

  // Limit the frequency content of the signal to avoid aliasing.
  anti_aliasing_filter_.Process(in, x);

  // Reduce the impact of near-end noise.
  noise_reduction_filter_.Process(x);

  // Downsample the signal.
  for (size_t j = 0, k = 0; j < out.size(); ++j, k += down_sampling_factor_) {
    RTC_DCHECK_GT(kBlockSize, k);
    out[j] = x[k];
  }
}

}  // namespace webrtc
