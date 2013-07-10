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

#ifndef TALK_BASE_BANDWIDTHSMOOTHER_H_
#define TALK_BASE_BANDWIDTHSMOOTHER_H_

#include "talk/base/rollingaccumulator.h"
#include "talk/base/timeutils.h"

namespace talk_base {

// The purpose of BandwidthSmoother is to smooth out bandwidth
// estimations so that 'trstate' messages can be triggered when we
// are "sure" there is sufficient bandwidth.  To avoid frequent fluctuations,
// we take a slightly pessimistic view of our bandwidth.  We only increase
// our estimation when we have sampled bandwidth measurements of values
// at least as large as the current estimation * percent_increase
// for at least time_between_increase time.  If a sampled bandwidth
// is less than our current estimation we immediately decrease our estimation
// to that sampled value.
// We retain the initial bandwidth guess as our current bandwidth estimation
// until we have received (min_sample_count_percent * samples_count_to_average)
// number of samples. Min_sample_count_percent must be in range [0, 1].
class BandwidthSmoother {
 public:
  BandwidthSmoother(int initial_bandwidth_guess,
                    uint32 time_between_increase,
                    double percent_increase,
                    size_t samples_count_to_average,
                    double min_sample_count_percent);

  // Samples a new bandwidth measurement.
  // bandwidth is expected to be non-negative.
  // returns true if the bandwidth estimation changed
  bool Sample(uint32 sample_time, int bandwidth);

  int get_bandwidth_estimation() const {
    return bandwidth_estimation_;
  }

 private:
  uint32 time_between_increase_;
  double percent_increase_;
  uint32 time_at_last_change_;
  int bandwidth_estimation_;
  RollingAccumulator<int> accumulator_;
  double min_sample_count_percent_;
};

}  // namespace talk_base

#endif  // TALK_BASE_BANDWIDTHSMOOTHER_H_
