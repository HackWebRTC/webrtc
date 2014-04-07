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

#include <algorithm>
#include <list>

#include "webrtc/modules/video_coding/utility/include/exp_filter.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

// TODO(mflodman) Test different values for all of these to trigger correctly,
// avoid fluctuations etc.
namespace {
const int64_t kProcessIntervalMs = 5000;

// Weight factor to apply to the standard deviation.
const float kWeightFactor = 0.997f;
// Weight factor to apply to the average.
const float kWeightFactorMean = 0.98f;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 30 * 1000;
const int kMaxRampUpDelayMs = 120 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// The initial average encode time (set to a fairly small value).
const float kInitialAvgEncodeTimeMs = 5.0f;

// The maximum exponent to use in VCMExpFilter.
const float kSampleDiffMs = 33.0f;
const float kMaxExp = 7.0f;

}  // namespace

Statistics::Statistics() :
    sum_(0.0),
    count_(0),
    filtered_samples_(new VCMExpFilter(kWeightFactorMean)),
    filtered_variance_(new VCMExpFilter(kWeightFactor)) {
  Reset();
}

void Statistics::SetOptions(const CpuOveruseOptions& options) {
  options_ = options;
}

void Statistics::Reset() {
  sum_ =  0.0;
  count_ = 0;
  filtered_variance_->Reset(kWeightFactor);
  filtered_variance_->Apply(1.0f, InitialVariance());
}

void Statistics::AddSample(float sample_ms) {
  sum_ += sample_ms;
  ++count_;

  if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
    // Initialize filtered samples.
    filtered_samples_->Reset(kWeightFactorMean);
    filtered_samples_->Apply(1.0f, InitialMean());
    return;
  }

  float exp = sample_ms / kSampleDiffMs;
  exp = std::min(exp, kMaxExp);
  filtered_samples_->Apply(exp, sample_ms);
  filtered_variance_->Apply(exp, (sample_ms - filtered_samples_->Value()) *
                                 (sample_ms - filtered_samples_->Value()));
}

float Statistics::InitialMean() const {
  if (count_ == 0)
    return 0.0;
  return sum_ / count_;
}

float Statistics::InitialVariance() const {
  // Start in between the underuse and overuse threshold.
  float average_stddev = (options_.low_capture_jitter_threshold_ms +
                          options_.high_capture_jitter_threshold_ms) / 2.0f;
  return average_stddev * average_stddev;
}

float Statistics::Mean() const { return filtered_samples_->Value(); }

float Statistics::StdDev() const {
  return sqrt(std::max(filtered_variance_->Value(), 0.0f));
}

uint64_t Statistics::Count() const { return count_; }


// Class for calculating the average encode time.
class OveruseFrameDetector::EncodeTimeAvg {
 public:
  EncodeTimeAvg()
      : kWeightFactor(0.5f),
        filtered_encode_time_ms_(new VCMExpFilter(kWeightFactor)) {
    filtered_encode_time_ms_->Apply(1.0f, kInitialAvgEncodeTimeMs);
  }
  ~EncodeTimeAvg() {}

  void AddEncodeSample(float encode_time_ms, int64_t diff_last_sample_ms) {
    float exp =  diff_last_sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_encode_time_ms_->Apply(exp, encode_time_ms);
  }

  int filtered_encode_time_ms() const {
    return static_cast<int>(filtered_encode_time_ms_->Value() + 0.5);
  }

 private:
  const float kWeightFactor;
  scoped_ptr<VCMExpFilter> filtered_encode_time_ms_;
};

// Class for calculating the encode usage.
class OveruseFrameDetector::EncodeUsage {
 public:
  EncodeUsage()
      : kWeightFactorFrameDiff(0.998f),
        kWeightFactorEncodeTime(0.995f),
        kInitialSampleDiffMs(50.0f),
        kMaxSampleDiffMs(66.0f),
        count_(0),
        filtered_encode_time_ms_(new VCMExpFilter(kWeightFactorEncodeTime)),
        filtered_frame_diff_ms_(new VCMExpFilter(kWeightFactorFrameDiff)) {
    Reset();
  }
  ~EncodeUsage() {}

  void SetOptions(const CpuOveruseOptions& options) {
    options_ = options;
  }

  void Reset() {
    count_ = 0;
    filtered_frame_diff_ms_->Reset(kWeightFactorFrameDiff);
    filtered_frame_diff_ms_->Apply(1.0f, kInitialSampleDiffMs);
    filtered_encode_time_ms_->Reset(kWeightFactorEncodeTime);
    filtered_encode_time_ms_->Apply(1.0f, InitialEncodeTimeMs());
  }

  void AddSample(float sample_ms) {
    float exp = sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_frame_diff_ms_->Apply(exp, sample_ms);
  }

  void AddEncodeSample(float encode_time_ms, int64_t diff_last_sample_ms) {
    ++count_;
    float exp = diff_last_sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_encode_time_ms_->Apply(exp, encode_time_ms);
  }

  int UsageInPercent() const {
    if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
      return static_cast<int>(InitialUsageInPercent() + 0.5f);
    }
    float frame_diff_ms = std::max(filtered_frame_diff_ms_->Value(), 1.0f);
    frame_diff_ms = std::min(frame_diff_ms, kMaxSampleDiffMs);
    float encode_usage_percent =
        100.0f * filtered_encode_time_ms_->Value() / frame_diff_ms;
    return static_cast<int>(encode_usage_percent + 0.5);
  }

  float InitialUsageInPercent() const {
    // Start in between the underuse and overuse threshold.
    return (options_.low_encode_usage_threshold_percent +
            options_.high_encode_usage_threshold_percent) / 2.0f;
  }

  float InitialEncodeTimeMs() const {
    return InitialUsageInPercent() * kInitialSampleDiffMs / 100;
  }

 private:
  const float kWeightFactorFrameDiff;
  const float kWeightFactorEncodeTime;
  const float kInitialSampleDiffMs;
  const float kMaxSampleDiffMs;
  uint64_t count_;
  CpuOveruseOptions options_;
  scoped_ptr<VCMExpFilter> filtered_encode_time_ms_;
  scoped_ptr<VCMExpFilter> filtered_frame_diff_ms_;
};

// Class for calculating the capture queue delay change.
class OveruseFrameDetector::CaptureQueueDelay {
 public:
  CaptureQueueDelay()
      : kWeightFactor(0.5f),
        delay_ms_(0),
        filtered_delay_ms_per_s_(new VCMExpFilter(kWeightFactor)) {
    filtered_delay_ms_per_s_->Apply(1.0f, 0.0f);
  }
  ~CaptureQueueDelay() {}

  void FrameCaptured(int64_t now) {
    const size_t kMaxSize = 200;
    if (frames_.size() > kMaxSize) {
      frames_.pop_front();
    }
    frames_.push_back(now);
  }

  void FrameProcessingStarted(int64_t now) {
    if (frames_.empty()) {
      return;
    }
    delay_ms_ = now - frames_.front();
    frames_.pop_front();
  }

  void CalculateDelayChange(int64_t diff_last_sample_ms) {
    if (diff_last_sample_ms <= 0) {
      return;
    }
    float exp = static_cast<float>(diff_last_sample_ms) / kProcessIntervalMs;
    exp = std::min(exp, kMaxExp);
    filtered_delay_ms_per_s_->Apply(exp,
                                    delay_ms_ * 1000.0f / diff_last_sample_ms);
    ClearFrames();
  }

  void ClearFrames() {
    frames_.clear();
  }

  int delay_ms() const {
    return delay_ms_;
  }

  int filtered_delay_ms_per_s() const {
    return static_cast<int>(filtered_delay_ms_per_s_->Value() + 0.5);
  }

 private:
  const float kWeightFactor;
  std::list<int64_t> frames_;
  int delay_ms_;
  scoped_ptr<VCMExpFilter> filtered_delay_ms_per_s_;
};

OveruseFrameDetector::OveruseFrameDetector(Clock* clock)
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      clock_(clock),
      next_process_time_(clock_->TimeInMilliseconds()),
      num_process_times_(0),
      last_capture_time_(0),
      last_overuse_time_(0),
      checks_above_threshold_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      num_pixels_(0),
      last_encode_sample_ms_(0),
      encode_time_(new EncodeTimeAvg()),
      encode_usage_(new EncodeUsage()),
      capture_queue_delay_(new CaptureQueueDelay()) {
}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  observer_ = observer;
}

void OveruseFrameDetector::SetOptions(const CpuOveruseOptions& options) {
  assert(options.min_frame_samples > 0);
  CriticalSectionScoped cs(crit_.get());
  if (options_.Equals(options)) {
    return;
  }
  options_ = options;
  capture_deltas_.SetOptions(options);
  encode_usage_->SetOptions(options);
  ResetAll(num_pixels_);
}

int OveruseFrameDetector::CaptureJitterMs() const {
  CriticalSectionScoped cs(crit_.get());
  return static_cast<int>(capture_deltas_.StdDev() + 0.5);
}

int OveruseFrameDetector::AvgEncodeTimeMs() const {
  CriticalSectionScoped cs(crit_.get());
  return encode_time_->filtered_encode_time_ms();
}

int OveruseFrameDetector::EncodeUsagePercent() const {
  CriticalSectionScoped cs(crit_.get());
  return encode_usage_->UsageInPercent();
}

int OveruseFrameDetector::AvgCaptureQueueDelayMsPerS() const {
  CriticalSectionScoped cs(crit_.get());
  return capture_queue_delay_->filtered_delay_ms_per_s();
}

int OveruseFrameDetector::CaptureQueueDelayMsPerS() const {
  CriticalSectionScoped cs(crit_.get());
  return capture_queue_delay_->delay_ms();
}

int32_t OveruseFrameDetector::TimeUntilNextProcess() {
  CriticalSectionScoped cs(crit_.get());
  return next_process_time_ - clock_->TimeInMilliseconds();
}

bool OveruseFrameDetector::FrameSizeChanged(int num_pixels) const {
  if (num_pixels != num_pixels_) {
    return true;
  }
  return false;
}

bool OveruseFrameDetector::FrameTimeoutDetected(int64_t now) const {
  if (last_capture_time_ == 0) {
    return false;
  }
  return (now - last_capture_time_) > options_.frame_timeout_interval_ms;
}

void OveruseFrameDetector::ResetAll(int num_pixels) {
  num_pixels_ = num_pixels;
  capture_deltas_.Reset();
  encode_usage_->Reset();
  capture_queue_delay_->ClearFrames();
  last_capture_time_ = 0;
  num_process_times_ = 0;
}

void OveruseFrameDetector::FrameCaptured(int width, int height) {
  CriticalSectionScoped cs(crit_.get());

  int64_t now = clock_->TimeInMilliseconds();
  if (FrameSizeChanged(width * height) || FrameTimeoutDetected(now)) {
    ResetAll(width * height);
  }

  if (last_capture_time_ != 0) {
    capture_deltas_.AddSample(now - last_capture_time_);
    encode_usage_->AddSample(now - last_capture_time_);
  }
  last_capture_time_ = now;

  capture_queue_delay_->FrameCaptured(now);
}

void OveruseFrameDetector::FrameProcessingStarted() {
  CriticalSectionScoped cs(crit_.get());
  capture_queue_delay_->FrameProcessingStarted(clock_->TimeInMilliseconds());
}

void OveruseFrameDetector::FrameEncoded(int encode_time_ms) {
  CriticalSectionScoped cs(crit_.get());
  int64_t time = clock_->TimeInMilliseconds();
  if (last_encode_sample_ms_ != 0) {
    int64_t diff_ms = time - last_encode_sample_ms_;
    encode_time_->AddEncodeSample(encode_time_ms, diff_ms);
    encode_usage_->AddEncodeSample(encode_time_ms, diff_ms);
  }
  last_encode_sample_ms_ = time;
}

int32_t OveruseFrameDetector::Process() {
  CriticalSectionScoped cs(crit_.get());

  int64_t now = clock_->TimeInMilliseconds();

  // Used to protect against Process() being called too often.
  if (now < next_process_time_)
    return 0;

  int64_t diff_ms = now - next_process_time_ + kProcessIntervalMs;
  next_process_time_ = now + kProcessIntervalMs;
  ++num_process_times_;

  capture_queue_delay_->CalculateDelayChange(diff_ms);

  if (num_process_times_ <= options_.min_process_count) {
    return 0;
  }

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

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  LOG(LS_VERBOSE) << "Capture input stats: avg: " << capture_deltas_.Mean()
                  << " std_dev " << capture_deltas_.StdDev()
                  << " rampup delay " << rampup_delay
                  << " overuse >= " << options_.high_capture_jitter_threshold_ms
                  << " underuse < " << options_.low_capture_jitter_threshold_ms;
  return 0;
}

bool OveruseFrameDetector::IsOverusing() {
  bool overusing = false;
  if (options_.enable_capture_jitter_method) {
    overusing = capture_deltas_.StdDev() >=
        options_.high_capture_jitter_threshold_ms;
  } else if (options_.enable_encode_usage_method) {
    overusing = encode_usage_->UsageInPercent() >=
        options_.high_encode_usage_threshold_percent;
  }

  if (overusing) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(int64_t time_now) {
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ + delay)
    return false;

  bool underusing = false;
  if (options_.enable_capture_jitter_method) {
    underusing = capture_deltas_.StdDev() <
        options_.low_capture_jitter_threshold_ms;
  } else if (options_.enable_encode_usage_method) {
    underusing = encode_usage_->UsageInPercent() <
        options_.low_encode_usage_threshold_percent;
  }
  return underusing;
}
}  // namespace webrtc
