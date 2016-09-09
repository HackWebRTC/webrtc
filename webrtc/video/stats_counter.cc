/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/stats_counter.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

namespace {
// Periodic time interval for processing samples.
const int64_t kProcessIntervalMs = 2000;
}  // namespace

// Class holding periodically computed metrics.
class AggregatedCounter {
 public:
  AggregatedCounter() : last_sample_(0), sum_samples_(0) {}
  ~AggregatedCounter() {}

  void Add(int sample) {
    last_sample_ = sample;
    sum_samples_ += sample;
    ++stats_.num_samples;
    if (stats_.num_samples == 1) {
      stats_.min = sample;
      stats_.max = sample;
    }
    stats_.min = std::min(sample, stats_.min);
    stats_.max = std::max(sample, stats_.max);
  }

  AggregatedStats ComputeStats() {
    Compute();
    return stats_;
  }

  bool Empty() const { return stats_.num_samples == 0; }

  int last_sample() const { return last_sample_; }

 private:
  void Compute() {
    if (stats_.num_samples == 0)
      return;

    stats_.average =
        (sum_samples_ + stats_.num_samples / 2) / stats_.num_samples;
  }
  int last_sample_;
  int64_t sum_samples_;
  AggregatedStats stats_;
};

// StatsCounter class.
StatsCounter::StatsCounter(Clock* clock,
                           bool include_empty_intervals,
                           StatsCounterObserver* observer)
    : max_(0),
      sum_(0),
      num_samples_(0),
      last_sum_(0),
      aggregated_counter_(new AggregatedCounter()),
      clock_(clock),
      include_empty_intervals_(include_empty_intervals),
      observer_(observer),
      last_process_time_ms_(-1),
      paused_(false) {}

StatsCounter::~StatsCounter() {}

AggregatedStats StatsCounter::GetStats() {
  return aggregated_counter_->ComputeStats();
}

AggregatedStats StatsCounter::ProcessAndGetStats() {
  if (HasSample())
    TryProcess();
  return aggregated_counter_->ComputeStats();
}

void StatsCounter::ProcessAndPause() {
  if (HasSample())
    TryProcess();
  paused_ = true;
}

bool StatsCounter::HasSample() const {
  return last_process_time_ms_ != -1;
}

bool StatsCounter::TimeToProcess(int* elapsed_intervals) {
  int64_t now = clock_->TimeInMilliseconds();
  if (last_process_time_ms_ == -1)
    last_process_time_ms_ = now;

  int64_t diff_ms = now - last_process_time_ms_;
  if (diff_ms < kProcessIntervalMs)
    return false;

  // Advance number of complete kProcessIntervalMs that have passed.
  int64_t num_intervals = diff_ms / kProcessIntervalMs;
  last_process_time_ms_ += num_intervals * kProcessIntervalMs;

  *elapsed_intervals = num_intervals;
  return true;
}

void StatsCounter::Set(int sample) {
  TryProcess();
  ++num_samples_;
  sum_ = sample;
  paused_ = false;
}

void StatsCounter::Add(int sample) {
  TryProcess();
  ++num_samples_;
  sum_ += sample;

  if (num_samples_ == 1)
    max_ = sample;
  max_ = std::max(sample, max_);
  paused_ = false;
}

// Reports periodically computed metric.
void StatsCounter::ReportMetricToAggregatedCounter(
    int value,
    int num_values_to_add) const {
  for (int i = 0; i < num_values_to_add; ++i) {
    aggregated_counter_->Add(value);
    if (observer_)
      observer_->OnMetricUpdated(value);
  }
}

void StatsCounter::TryProcess() {
  int elapsed_intervals;
  if (!TimeToProcess(&elapsed_intervals))
    return;

  // Get and report periodically computed metric.
  int metric;
  if (GetMetric(&metric))
    ReportMetricToAggregatedCounter(metric, 1);

  // Report value for elapsed intervals without samples.
  if (IncludeEmptyIntervals()) {
    // If there are no samples, all elapsed intervals are empty (otherwise one
    // interval contains sample(s), discard this interval).
    int empty_intervals =
        (num_samples_ == 0) ? elapsed_intervals : (elapsed_intervals - 1);
    ReportMetricToAggregatedCounter(GetValueForEmptyInterval(),
                                    empty_intervals);
  }

  // Reset samples for elapsed interval.
  if (num_samples_ > 0)
    last_sum_ = sum_;
  sum_ = 0;
  max_ = 0;
  num_samples_ = 0;
}

bool StatsCounter::IncludeEmptyIntervals() const {
  return include_empty_intervals_ && !paused_ && !aggregated_counter_->Empty();
}

// StatsCounter sub-classes.
AvgCounter::AvgCounter(Clock* clock,
                       StatsCounterObserver* observer,
                       bool include_empty_intervals)
    : StatsCounter(clock, include_empty_intervals, observer) {}

void AvgCounter::Add(int sample) {
  StatsCounter::Add(sample);
}

bool AvgCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0)
    return false;
  *metric = (sum_ + num_samples_ / 2) / num_samples_;
  return true;
}

int AvgCounter::GetValueForEmptyInterval() const {
  return aggregated_counter_->last_sample();
}

MaxCounter::MaxCounter(Clock* clock, StatsCounterObserver* observer)
    : StatsCounter(clock,
                   false,  // |include_empty_intervals|
                   observer) {}

void MaxCounter::Add(int sample) {
  StatsCounter::Add(sample);
}

bool MaxCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0)
    return false;
  *metric = max_;
  return true;
}

int MaxCounter::GetValueForEmptyInterval() const {
  RTC_NOTREACHED();
  return 0;
}

PercentCounter::PercentCounter(Clock* clock, StatsCounterObserver* observer)
    : StatsCounter(clock,
                   false,  // |include_empty_intervals|
                   observer) {}

void PercentCounter::Add(bool sample) {
  StatsCounter::Add(sample ? 1 : 0);
}

bool PercentCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0)
    return false;
  *metric = (sum_ * 100 + num_samples_ / 2) / num_samples_;
  return true;
}

int PercentCounter::GetValueForEmptyInterval() const {
  RTC_NOTREACHED();
  return 0;
}

PermilleCounter::PermilleCounter(Clock* clock, StatsCounterObserver* observer)
    : StatsCounter(clock,
                   false,  // |include_empty_intervals|
                   observer) {}

void PermilleCounter::Add(bool sample) {
  StatsCounter::Add(sample ? 1 : 0);
}

bool PermilleCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0)
    return false;
  *metric = (sum_ * 1000 + num_samples_ / 2) / num_samples_;
  return true;
}

int PermilleCounter::GetValueForEmptyInterval() const {
  RTC_NOTREACHED();
  return 0;
}

RateCounter::RateCounter(Clock* clock,
                         StatsCounterObserver* observer,
                         bool include_empty_intervals)
    : StatsCounter(clock, include_empty_intervals, observer) {}

void RateCounter::Add(int sample) {
  StatsCounter::Add(sample);
}

bool RateCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0)
    return false;
  *metric = (sum_ * 1000 + kProcessIntervalMs / 2) / kProcessIntervalMs;
  return true;
}

int RateCounter::GetValueForEmptyInterval() const {
  return 0;
}

RateAccCounter::RateAccCounter(Clock* clock,
                               StatsCounterObserver* observer,
                               bool include_empty_intervals)
    : StatsCounter(clock, include_empty_intervals, observer) {}

void RateAccCounter::Set(int sample) {
  StatsCounter::Set(sample);
}

bool RateAccCounter::GetMetric(int* metric) const {
  if (num_samples_ == 0 || last_sum_ > sum_)
    return false;
  *metric =
      ((sum_ - last_sum_) * 1000 + kProcessIntervalMs / 2) / kProcessIntervalMs;
  return true;
}

int RateAccCounter::GetValueForEmptyInterval() const {
  return 0;
}

}  // namespace webrtc
