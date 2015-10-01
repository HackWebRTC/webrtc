/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "talk/app/webrtc/fakemetricsobserver.h"
#include "webrtc/base/checks.h"

namespace webrtc {

FakeMetricsObserver::FakeMetricsObserver() {
  Reset();
}

void FakeMetricsObserver::Reset() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  counters_.clear();
  memset(histogram_samples_, 0, sizeof(histogram_samples_));
}

void FakeMetricsObserver::IncrementEnumCounter(
    PeerConnectionEnumCounterType type,
    int counter,
    int counter_max) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (counters_.size() <= static_cast<size_t>(type)) {
    counters_.resize(type + 1);
  }
  auto& counters = counters_[type];
  ++counters[counter];
}

void FakeMetricsObserver::AddHistogramSample(PeerConnectionMetricsName type,
    int value) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK_EQ(histogram_samples_[type], 0);
  histogram_samples_[type] = value;
}

int FakeMetricsObserver::GetEnumCounter(PeerConnectionEnumCounterType type,
                                        int counter) const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_CHECK(counters_.size() > static_cast<size_t>(type));
  const auto& it = counters_[type].find(counter);
  if (it == counters_[type].end()) {
    return 0;
  }
  return it->second;
}

int FakeMetricsObserver::GetHistogramSample(
    PeerConnectionMetricsName type) const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return histogram_samples_[type];
}

}  // namespace webrtc
