/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/matched_filter.h"

#include <algorithm>
#include <numeric>

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

MatchedFilter::IndexedBuffer::IndexedBuffer(size_t size) : data(size, 0.f) {
  RTC_DCHECK_EQ(0, size % kSubBlockSize);
}

MatchedFilter::IndexedBuffer::~IndexedBuffer() = default;

MatchedFilter::MatchedFilter(ApmDataDumper* data_dumper,
                             size_t window_size_sub_blocks,
                             int num_matched_filters,
                             size_t alignment_shift_sub_blocks)
    : data_dumper_(data_dumper),
      filter_intra_lag_shift_(alignment_shift_sub_blocks * kSubBlockSize),
      filters_(num_matched_filters,
               std::vector<float>(window_size_sub_blocks * kSubBlockSize, 0.f)),
      lag_estimates_(num_matched_filters),
      x_buffer_(kSubBlockSize *
                (alignment_shift_sub_blocks * num_matched_filters +
                 window_size_sub_blocks +
                 1)) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK_EQ(0, x_buffer_.data.size() % kSubBlockSize);
  RTC_DCHECK_LT(0, window_size_sub_blocks);
}

MatchedFilter::~MatchedFilter() = default;

void MatchedFilter::Update(const std::array<float, kSubBlockSize>& render,
                           const std::array<float, kSubBlockSize>& capture) {
  const std::array<float, kSubBlockSize>& x = render;
  const std::array<float, kSubBlockSize>& y = capture;

  const float x2_sum_threshold = filters_[0].size() * 150.f * 150.f;

  // Insert the new subblock into x_buffer.
  x_buffer_.index = (x_buffer_.index - kSubBlockSize + x_buffer_.data.size()) %
                    x_buffer_.data.size();
  RTC_DCHECK_LE(kSubBlockSize, x_buffer_.data.size() - x_buffer_.index);
  std::copy(x.rbegin(), x.rend(), x_buffer_.data.begin() + x_buffer_.index);

  // Apply all matched filters.
  size_t alignment_shift = 0;
  for (size_t n = 0; n < filters_.size(); ++n) {
    float error_sum = 0.f;
    bool filters_updated = false;
    size_t x_start_index =
        (x_buffer_.index + alignment_shift + kSubBlockSize - 1) %
        x_buffer_.data.size();

    // Process for all samples in the sub-block.
    for (size_t i = 0; i < kSubBlockSize; ++i) {
      // As x_buffer is a circular buffer, all of the processing is split into
      // two loops around the wrapping of the buffer.
      const size_t loop_size_1 =
          std::min(filters_[n].size(), x_buffer_.data.size() - x_start_index);
      const size_t loop_size_2 = filters_[n].size() - loop_size_1;
      RTC_DCHECK_EQ(filters_[n].size(), loop_size_1 + loop_size_2);

      // x * x.
      float x2_sum = std::inner_product(
          x_buffer_.data.begin() + x_start_index,
          x_buffer_.data.begin() + x_start_index + loop_size_1,
          x_buffer_.data.begin() + x_start_index, 0.f);
      // Apply the matched filter as filter * x.
      float s = std::inner_product(filters_[n].begin(),
                                   filters_[n].begin() + loop_size_1,
                                   x_buffer_.data.begin() + x_start_index, 0.f);

      if (loop_size_2 > 0) {
        // Update the cumulative sum of x * x.
        x2_sum = std::inner_product(x_buffer_.data.begin(),
                                    x_buffer_.data.begin() + loop_size_2,
                                    x_buffer_.data.begin(), x2_sum);

        // Compute the matched filter output filter * x in a cumulative manner.
        s = std::inner_product(x_buffer_.data.begin(),
                               x_buffer_.data.begin() + loop_size_2,
                               filters_[n].begin() + loop_size_1, s);
      }

      // Compute the matched filter error.
      const float e = std::min(32767.f, std::max(-32768.f, y[i] - s));
      error_sum += e * e;

      // Update the matched filter estimate in an NLMS manner.
      if (x2_sum > x2_sum_threshold) {
        filters_updated = true;
        RTC_DCHECK_LT(0.f, x2_sum);
        const float alpha = 0.7f * e / x2_sum;

        // filter = filter + 0.7 * (y - filter * x) / x * x.
        std::transform(filters_[n].begin(), filters_[n].begin() + loop_size_1,
                       x_buffer_.data.begin() + x_start_index,
                       filters_[n].begin(),
                       [&](float a, float b) { return a + alpha * b; });

        if (loop_size_2 > 0) {
          // filter = filter + 0.7 * (y - filter * x) / x * x.
          std::transform(x_buffer_.data.begin(),
                         x_buffer_.data.begin() + loop_size_2,
                         filters_[n].begin() + loop_size_1,
                         filters_[n].begin() + loop_size_1,
                         [&](float a, float b) { return b + alpha * a; });
        }
      }

      x_start_index =
          x_start_index > 0 ? x_start_index - 1 : x_buffer_.data.size() - 1;
    }

    // Compute anchor for the matched filter error.
    const float error_sum_anchor =
        std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

    // Estimate the lag in the matched filter as the distance to the portion in
    // the filter that contributes the most to the matched filter output. This
    // is detected as the peak of the matched filter.
    const size_t lag_estimate = std::distance(
        filters_[n].begin(),
        std::max_element(
            filters_[n].begin(), filters_[n].end(),
            [](float a, float b) -> bool { return a * a < b * b; }));

    // Update the lag estimates for the matched filter.
    const float kMatchingFilterThreshold = 0.3f;
    lag_estimates_[n] =
        LagEstimate(error_sum_anchor - error_sum,
                    error_sum < kMatchingFilterThreshold * error_sum_anchor,
                    lag_estimate + alignment_shift, filters_updated);

    // TODO(peah): Remove once development of EchoCanceller3 is fully done.
    RTC_DCHECK_EQ(4, filters_.size());
    switch (n) {
      case 0:
        data_dumper_->DumpRaw("aec3_correlator_0_h", filters_[0]);
        break;
      case 1:
        data_dumper_->DumpRaw("aec3_correlator_1_h", filters_[1]);
        break;
      case 2:
        data_dumper_->DumpRaw("aec3_correlator_2_h", filters_[2]);
        break;
      case 3:
        data_dumper_->DumpRaw("aec3_correlator_3_h", filters_[3]);
        break;
      default:
        RTC_DCHECK(false);
    }

    alignment_shift += filter_intra_lag_shift_;
  }
}

}  // namespace webrtc
