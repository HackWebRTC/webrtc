/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_
#define RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_

#include <algorithm>
#include <cmath>
#include <limits>

#include "absl/types/optional.h"

#include "rtc_base/numerics/math_utils.h"

namespace webrtc {

// tl;dr: Robust and efficient online computation of statistics,
//        using Welford's method for variance. [1]
//
// This should be your go-to class if you ever need to compute
// min, max, mean, variance and standard deviation.
// If you need to get percentiles, please use webrtc::SamplesStatsCounter.
//
// The measures return absl::nullopt if no samples were fed (Size() == 0),
// otherwise the returned optional is guaranteed to contain a value.
//
// [1]
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm

// The type T is a scalar which must be convertible to double.
// Rationale: we often need greater precision for measures
//            than for the samples themselves.
template <typename T>
class RunningStatistics {
 public:
  // Update stats ////////////////////////////////////////////

  // Add a value participating in the statistics in O(1) time.
  void AddSample(T sample) {
    max_ = std::max(max_, sample);
    min_ = std::min(min_, sample);
    ++size_;
    // Welford's incremental update.
    const double delta = sample - mean_;
    mean_ += delta / size_;
    const double delta2 = sample - mean_;
    cumul_ += delta * delta2;
  }

  // Merge other stats, as if samples were added one by one, but in O(1).
  void MergeStatistics(const RunningStatistics<T>& other) {
    if (other.size_ == 0) {
      return;
    }
    max_ = std::max(max_, other.max_);
    min_ = std::min(min_, other.min_);
    const int64_t new_size = size_ + other.size_;
    const double new_mean =
        (mean_ * size_ + other.mean_ * other.size_) / new_size;
    // Each cumulant must be corrected.
    //   * from: sum((x_i - mean_)²)
    //   * to:   sum((x_i - new_mean)²)
    auto delta = [new_mean](const RunningStatistics<T>& stats) {
      return stats.size_ * (new_mean * (new_mean - 2 * stats.mean_) +
                            stats.mean_ * stats.mean_);
    };
    cumul_ = cumul_ + delta(*this) + other.cumul_ + delta(other);
    mean_ = new_mean;
    size_ = new_size;
  }

  // Get Measures ////////////////////////////////////////////

  // Returns number of samples involved,
  // that is number of times AddSample() was called.
  int64_t Size() const { return size_; }

  // Returns min in O(1) time.
  absl::optional<T> GetMin() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return min_;
  }

  // Returns max in O(1) time.
  absl::optional<T> GetMax() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return max_;
  }

  // Returns mean in O(1) time.
  absl::optional<double> GetMean() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return mean_;
  }

  // Returns unbiased sample variance in O(1) time.
  absl::optional<double> GetVariance() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return cumul_ / size_;
  }

  // Returns unbiased standard deviation in O(1) time.
  absl::optional<double> GetStandardDeviation() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return std::sqrt(*GetVariance());
  }

 private:
  int64_t size_ = 0;  // Samples seen.
  T min_ = infinity_or_max<T>();
  T max_ = minus_infinity_or_min<T>();
  double mean_ = 0;
  double cumul_ = 0;  // Variance * size_, sometimes noted m2.
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_
