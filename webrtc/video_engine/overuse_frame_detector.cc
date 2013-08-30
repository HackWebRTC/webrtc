/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/overuse_frame_detector.h"

#include <assert.h>
#include <math.h>

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

Statistics::Statistics() { Reset(); }

void Statistics::Reset() {
  sum_ = sum_squared_ = 0.0;
  count_ = 0;
}

void Statistics::AddSample(double sample) {
  sum_ += sample;
  sum_squared_ += sample * sample;
  ++count_;
}

double Statistics::Mean() const {
  if (count_ == 0)
    return 0.0;
  return sum_ / count_;
}

double Statistics::Variance() const {
  if (count_ == 0)
    return 0.0;
  return sum_squared_ / count_ - Mean() * Mean();
}

double Statistics::StdDev() const { return sqrt(Variance()); }

uint64_t Statistics::Samples() const { return count_; }

// TODO(mflodman) Test different values for all of these to trigger correctly,
// avoid fluctuations etc.
namespace {
const int64_t kProcessIntervalMs = 5000;

// Limits on standard deviation for under/overuse.
const double kOveruseStdDevMs = 15.0;
const double kNormalUseStdDevMs = 10.0;

// Rampdown checks.
const int kConsecutiveChecksAboveThreshold = 2;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 30 * 1000;
const int kMaxRampUpDelayMs = 120 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Minimum samples required to perform a check.
const size_t kMinFrameSampleCount = 30;
}  // namespace

OveruseFrameDetector::OveruseFrameDetector(Clock* clock)
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      clock_(clock),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_capture_time_(0),
      last_overuse_time_(0),
      checks_above_threshold_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs) {}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  observer_ = observer;
}

void OveruseFrameDetector::FrameCaptured() {
  CriticalSectionScoped cs(crit_.get());
  int64_t time = clock_->TimeInMilliseconds();
  if (last_capture_time_ != 0) {
    capture_deltas_.AddSample(time - last_capture_time_);
  }
  last_capture_time_ = time;
}

int32_t OveruseFrameDetector::TimeUntilNextProcess() {
  CriticalSectionScoped cs(crit_.get());
  return next_process_time_ - clock_->TimeInMilliseconds();
}

int32_t OveruseFrameDetector::Process() {
  CriticalSectionScoped cs(crit_.get());

  int64_t now = clock_->TimeInMilliseconds();

  // Used to protect against Process() being called too often.
  if (now < next_process_time_)
    return 0;

  next_process_time_ = now + kProcessIntervalMs;

  // Don't trigger overuse unless we've seen any frames
  if (capture_deltas_.Samples() < kMinFrameSampleCount)
    return 0;

  if (IsOverusing()) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ > last_overuse_time_;
    if (check_for_backoff) {
      if (now - last_rampup_time_ < kStandardRampUpDelayMs) {
        // Going up was not ok for very long, back off.
        current_rampup_delay_ms_ *= kRampUpBackoffFactor;
        if (current_rampup_delay_ms_ > kMaxRampUpDelayMs)
          current_rampup_delay_ms_ = kMaxRampUpDelayMs;
      } else {
        // Not currently backing off, reset rampup delay.
        current_rampup_delay_ms_ = kStandardRampUpDelayMs;
      }
    }

    last_overuse_time_ = now;
    in_quick_rampup_ = false;
    checks_above_threshold_ = 0;

    if (observer_ != NULL)
      observer_->OveruseDetected();
  } else if (IsUnderusing(now)) {
    last_rampup_time_ = now;
    in_quick_rampup_ = true;

    if (observer_ != NULL)
      observer_->NormalUsage();
  }

  capture_deltas_.Reset();

  return 0;
}

bool OveruseFrameDetector::IsOverusing() {
  WEBRTC_TRACE(
      webrtc::kTraceInfo,
      webrtc::kTraceVideo,
      -1,
      "Capture input stats: avg: %.2fms, std_dev: %.2fms (rampup delay: "
      "%dms, overuse: >=%.2fms, "
      "underuse: <%.2fms)",
      capture_deltas_.Mean(),
      capture_deltas_.StdDev(),
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_,
      kOveruseStdDevMs,
      kNormalUseStdDevMs);

  if (capture_deltas_.StdDev() >= kOveruseStdDevMs) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }

  return checks_above_threshold_ >= kConsecutiveChecksAboveThreshold;
}

bool OveruseFrameDetector::IsUnderusing(int64_t time_now) {
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ + delay)
    return false;

  return capture_deltas_.StdDev() < kNormalUseStdDevMs;
}
}  // namespace webrtc
