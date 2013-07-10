/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_BASE_RATELIMITER_H_
#define TALK_BASE_RATELIMITER_H_

#include <stdlib.h>
#include "talk/base/basictypes.h"

namespace talk_base {

// Limits the rate of use to a certain maximum quantity per period of
// time.  Use, for example, for simple bandwidth throttling.
//
// It's implemented like a diet plan: You have so many calories per
// day.  If you hit the limit, you can't eat any more until the next
// day.
class RateLimiter {
 public:
  // For example, 100kb per second.
  RateLimiter(size_t max, double period)
      : max_per_period_(max),
        period_length_(period),
        used_in_period_(0),
        period_start_(0.0),
        period_end_(period) {
  }
  virtual ~RateLimiter() {}

  // Returns true if if the desired quantity is available in the
  // current period (< (max - used)).  Once the given time passes the
  // end of the period, used is set to zero and more use is available.
  bool CanUse(size_t desired, double time);
  // Increment the quantity used this period.  If past the end of a
  // period, a new period is started.
  void Use(size_t used, double time);

  size_t used_in_period() const {
    return used_in_period_;
  }

  size_t max_per_period() const {
    return max_per_period_;
  }

 private:
  size_t max_per_period_;
  double period_length_;
  size_t used_in_period_;
  double period_start_;
  double period_end_;
};

}  // namespace talk_base

#endif  // TALK_BASE_RATELIMITER_H_
