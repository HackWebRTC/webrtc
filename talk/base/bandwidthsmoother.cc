/*
 * libjingle
 * Copyright 2011, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/bandwidthsmoother.h"

#include <limits.h>

namespace talk_base {

BandwidthSmoother::BandwidthSmoother(int initial_bandwidth_guess,
                                     uint32 time_between_increase,
                                     double percent_increase,
                                     size_t samples_count_to_average,
                                     double min_sample_count_percent)
    : time_between_increase_(time_between_increase),
      percent_increase_(talk_base::_max(1.0, percent_increase)),
      time_at_last_change_(0),
      bandwidth_estimation_(initial_bandwidth_guess),
      accumulator_(samples_count_to_average),
      min_sample_count_percent_(
          talk_base::_min(1.0,
                          talk_base::_max(0.0, min_sample_count_percent))) {
}

// Samples a new bandwidth measurement
// returns true if the bandwidth estimation changed
bool BandwidthSmoother::Sample(uint32 sample_time, int bandwidth) {
  if (bandwidth < 0) {
    return false;
  }

  accumulator_.AddSample(bandwidth);

  if (accumulator_.count() < static_cast<size_t>(
          accumulator_.max_count() * min_sample_count_percent_)) {
    // We have not collected enough samples yet.
    return false;
  }

  // Replace bandwidth with the mean of sampled bandwidths.
  const int mean_bandwidth = static_cast<int>(accumulator_.ComputeMean());

  if (mean_bandwidth < bandwidth_estimation_) {
    time_at_last_change_ = sample_time;
    bandwidth_estimation_ = mean_bandwidth;
    return true;
  }

  const int old_bandwidth_estimation = bandwidth_estimation_;
  const double increase_threshold_d = percent_increase_ * bandwidth_estimation_;
  if (increase_threshold_d > INT_MAX) {
    // If bandwidth goes any higher we would overflow.
    return false;
  }

  const int increase_threshold = static_cast<int>(increase_threshold_d);
  if (mean_bandwidth < increase_threshold) {
    time_at_last_change_ = sample_time;
    // The value of bandwidth_estimation remains the same if we don't exceed
    // percent_increase_ * bandwidth_estimation_ for at least
    // time_between_increase_ time.
  } else if (sample_time >= time_at_last_change_ + time_between_increase_) {
    time_at_last_change_ = sample_time;
    if (increase_threshold == 0) {
      // Bandwidth_estimation_ must be zero. Assume a jump from zero to a
      // positive bandwidth means we have regained connectivity.
      bandwidth_estimation_ = mean_bandwidth;
    } else {
      bandwidth_estimation_ = increase_threshold;
    }
  }
  // Else don't make a change.

  return old_bandwidth_estimation != bandwidth_estimation_;
}

}  // namespace talk_base
