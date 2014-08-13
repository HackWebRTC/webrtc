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
#include <map>

#include "webrtc/base/exp_filter.h"
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
const int kStandardRampUpDelayMs = 40 * 1000;
const int kMaxRampUpDelayMs = 240 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Max number of overuses detected before always applying the rampup delay.
const int kMaxOverusesBeforeApplyRampupDelay = 4;

// The maximum exponent to use in VCMExpFilter.
const float kSampleDiffMs = 33.0f;
const float kMaxExp = 7.0f;

}  // namespace

Statistics::Statistics() :
    sum_(0.0),
    count_(0),
    filtered_samples_(new rtc::ExpFilter(kWeightFactorMean)),
    filtered_variance_(new rtc::ExpFilter(kWeightFactor)) {
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
  filtered_variance_->Apply(exp, (sample_ms - filtered_samples_->filtered()) *
                                 (sample_ms - filtered_samples_->filtered()));
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

float Statistics::Mean() const { return filtered_samples_->filtered(); }

float Statistics::StdDev() const {
  return sqrt(std::max(filtered_variance_->filtered(), 0.0f));
}

uint64_t Statistics::Count() const { return count_; }


// Class for calculating the average encode time.
class OveruseFrameDetector::EncodeTimeAvg {
 public:
  EncodeTimeAvg()
      : kWeightFactor(0.5f),
        kInitialAvgEncodeTimeMs(5.0f),
        filtered_encode_time_ms_(new rtc::ExpFilter(kWeightFactor)) {
    filtered_encode_time_ms_->Apply(1.0f, kInitialAvgEncodeTimeMs);
  }
  ~EncodeTimeAvg() {}

  void AddEncodeSample(float encode_time_ms, int64_t diff_last_sample_ms) {
    float exp =  diff_last_sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_encode_time_ms_->Apply(exp, encode_time_ms);
  }

  int Value() const {
    return static_cast<int>(filtered_encode_time_ms_->filtered() + 0.5);
  }

 private:
  const float kWeightFactor;
  const float kInitialAvgEncodeTimeMs;
  scoped_ptr<rtc::ExpFilter> filtered_encode_time_ms_;
};

// Class for calculating the encode usage.
class OveruseFrameDetector::EncodeUsage {
 public:
  EncodeUsage()
      : kWeightFactorFrameDiff(0.998f),
        kWeightFactorEncodeTime(0.995f),
        kInitialSampleDiffMs(40.0f),
        kMaxSampleDiffMs(45.0f),
        count_(0),
        filtered_encode_time_ms_(new rtc::ExpFilter(kWeightFactorEncodeTime)),
        filtered_frame_diff_ms_(new rtc::ExpFilter(kWeightFactorFrameDiff)) {
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

  int Value() const {
    if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
      return static_cast<int>(InitialUsageInPercent() + 0.5f);
    }
    float frame_diff_ms = std::max(filtered_frame_diff_ms_->filtered(), 1.0f);
    frame_diff_ms = std::min(frame_diff_ms, kMaxSampleDiffMs);
    float encode_usage_percent =
        100.0f * filtered_encode_time_ms_->filtered() / frame_diff_ms;
    return static_cast<int>(encode_usage_percent + 0.5);
  }

 private:
  float InitialUsageInPercent() const {
    // Start in between the underuse and overuse threshold.
    return (options_.low_encode_usage_threshold_percent +
            options_.high_encode_usage_threshold_percent) / 2.0f;
  }

  float InitialEncodeTimeMs() const {
    return InitialUsageInPercent() * kInitialSampleDiffMs / 100;
  }

  const float kWeightFactorFrameDiff;
  const float kWeightFactorEncodeTime;
  const float kInitialSampleDiffMs;
  const float kMaxSampleDiffMs;
  uint64_t count_;
  CpuOveruseOptions options_;
  scoped_ptr<rtc::ExpFilter> filtered_encode_time_ms_;
  scoped_ptr<rtc::ExpFilter> filtered_frame_diff_ms_;
};

// Class for calculating the relative standard deviation of encode times.
class OveruseFrameDetector::EncodeTimeRsd {
 public:
  EncodeTimeRsd(Clock* clock)
      : kWeightFactor(0.6f),
        count_(0),
        filtered_rsd_(new rtc::ExpFilter(kWeightFactor)),
        hist_samples_(0),
        hist_sum_(0.0f),
        last_process_time_ms_(clock->TimeInMilliseconds()) {
    Reset();
  }
  ~EncodeTimeRsd() {}

  void SetOptions(const CpuOveruseOptions& options) {
    options_ = options;
  }

  void Reset() {
    count_ = 0;
    filtered_rsd_->Reset(kWeightFactor);
    filtered_rsd_->Apply(1.0f, InitialValue());
    hist_.clear();
    hist_samples_ = 0;
    hist_sum_ = 0.0f;
  }

  void AddEncodeSample(float encode_time_ms) {
    int bin = static_cast<int>(encode_time_ms + 0.5f);
    if (bin <= 0) {
      // The frame was probably not encoded, skip possible dropped frame.
      return;
    }
    ++count_;
    ++hist_[bin];
    ++hist_samples_;
    hist_sum_ += bin;
  }

  void Process(int64_t now) {
    if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
      // Have not received min number of frames since last reset.
      return;
    }
    const int kMinHistSamples = 20;
    if (hist_samples_ < kMinHistSamples) {
      return;
    }
    const int64_t kMinDiffSinceLastProcessMs = 1000;
    int64_t diff_last_process_ms = now - last_process_time_ms_;
    if (now - last_process_time_ms_ <= kMinDiffSinceLastProcessMs) {
      return;
    }
    last_process_time_ms_ = now;

    // Calculate variance (using samples above the mean).
    // Checks for a larger encode time of some frames while there is a small
    // increase in the average time.
    int mean = hist_sum_ / hist_samples_;
    float variance = 0.0f;
    int total_count = 0;
    for (std::map<int,int>::iterator it = hist_.begin();
         it != hist_.end(); ++it) {
      int time = it->first;
      int count = it->second;
      if (time > mean) {
        total_count += count;
        for (int i = 0; i < count; ++i) {
          variance += ((time - mean) * (time - mean));
        }
      }
    }
    variance /= std::max(total_count, 1);
    float cov = sqrt(variance) / mean;

    hist_.clear();
    hist_samples_ = 0;
    hist_sum_ = 0.0f;

    float exp = static_cast<float>(diff_last_process_ms) / kProcessIntervalMs;
    exp = std::min(exp, kMaxExp);
    filtered_rsd_->Apply(exp, 100.0f * cov);
  }

  int Value() const {
    return static_cast<int>(filtered_rsd_->filtered() + 0.5);
  }

 private:
  float InitialValue() const {
    // Start in between the underuse and overuse threshold.
    return std::max(((options_.low_encode_time_rsd_threshold +
                      options_.high_encode_time_rsd_threshold) / 2.0f), 0.0f);
  }

  const float kWeightFactor;
  uint32_t count_;  // Number of encode samples since last reset.
  CpuOveruseOptions options_;
  scoped_ptr<rtc::ExpFilter> filtered_rsd_;
  int hist_samples_;
  float hist_sum_;
  std::map<int,int> hist_;  // Histogram of encode time of frames.
  int64_t last_process_time_ms_;
};

// Class for calculating the capture queue delay change.
class OveruseFrameDetector::CaptureQueueDelay {
 public:
  CaptureQueueDelay()
      : kWeightFactor(0.5f),
        delay_ms_(0),
        filtered_delay_ms_per_s_(new rtc::ExpFilter(kWeightFactor)) {
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

  int Value() const {
    return static_cast<int>(filtered_delay_ms_per_s_->filtered() + 0.5);
  }

 private:
  const float kWeightFactor;
  std::list<int64_t> frames_;
  int delay_ms_;
  scoped_ptr<rtc::ExpFilter> filtered_delay_ms_per_s_;
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
      num_overuse_detections_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      num_pixels_(0),
      last_encode_sample_ms_(0),
      encode_time_(new EncodeTimeAvg()),
      encode_rsd_(new EncodeTimeRsd(clock)),
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
  encode_rsd_->SetOptions(options);
  ResetAll(num_pixels_);
}

int OveruseFrameDetector::CaptureQueueDelayMsPerS() const {
  CriticalSectionScoped cs(crit_.get());
  return capture_queue_delay_->delay_ms();
}

void OveruseFrameDetector::GetCpuOveruseMetrics(
    CpuOveruseMetrics* metrics) const {
  CriticalSectionScoped cs(crit_.get());
  metrics->capture_jitter_ms = static_cast<int>(capture_deltas_.StdDev() + 0.5);
  metrics->avg_encode_time_ms = encode_time_->Value();
  metrics->encode_rsd = encode_rsd_->Value();
  metrics->encode_usage_percent = encode_usage_->Value();
  metrics->capture_queue_delay_ms_per_s = capture_queue_delay_->Value();
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
  encode_rsd_->Reset();
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
    encode_rsd_->AddEncodeSample(encode_time_ms);
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

  encode_rsd_->Process(now);
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
      if (now - last_rampup_time_ < kStandardRampUpDelayMs ||
          num_overuse_detections_ > kMaxOverusesBeforeApplyRampupDelay) {
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
    ++num_overuse_detections_;

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
  LOG(LS_VERBOSE) << " Frame stats: capture avg: " << capture_deltas_.Mean()
                  << " capture stddev " << capture_deltas_.StdDev()
                  << " encode usage " << encode_usage_->Value()
                  << " encode rsd " << encode_rsd_->Value()
                  << " overuse detections " << num_overuse_detections_
                  << " rampup delay " << rampup_delay;
  return 0;
}

bool OveruseFrameDetector::IsOverusing() {
  bool overusing = false;
  if (options_.enable_capture_jitter_method) {
    overusing = capture_deltas_.StdDev() >=
        options_.high_capture_jitter_threshold_ms;
  } else if (options_.enable_encode_usage_method) {
    bool encode_usage_overuse =
        encode_usage_->Value() >= options_.high_encode_usage_threshold_percent;
    bool encode_rsd_overuse = false;
    if (options_.high_encode_time_rsd_threshold > 0) {
      encode_rsd_overuse =
          (encode_rsd_->Value() >= options_.high_encode_time_rsd_threshold);
    }
    overusing = encode_usage_overuse || encode_rsd_overuse;
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
    bool encode_usage_underuse =
        encode_usage_->Value() < options_.low_encode_usage_threshold_percent;
    bool encode_rsd_underuse = true;
    if (options_.low_encode_time_rsd_threshold > 0) {
      encode_rsd_underuse =
          (encode_rsd_->Value() < options_.low_encode_time_rsd_threshold);
    }
    underusing = encode_usage_underuse && encode_rsd_underuse;
  }
  return underusing;
}
}  // namespace webrtc
