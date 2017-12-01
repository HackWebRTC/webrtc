/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector.h"

#include <assert.h>
#include <math.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "api/video/video_frame.h"
#include "common_video/include/frame_callback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <mach/mach.h>
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

namespace {
const int64_t kCheckForOveruseIntervalMs = 5000;
const int64_t kTimeToFirstCheckForOveruseMs = 100;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 40 * 1000;
const int kMaxRampUpDelayMs = 240 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Max number of overuses detected before always applying the rampup delay.
const int kMaxOverusesBeforeApplyRampupDelay = 4;

const auto kScaleReasonCpu = AdaptationObserverInterface::AdaptReason::kCpu;
}  // namespace

CpuOveruseOptions::CpuOveruseOptions()
    : high_encode_usage_threshold_percent(85),
      frame_timeout_interval_ms(1500),
      min_process_count(3),
      high_threshold_consecutive_count(2),
      filter_time_ms(5000) {
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  // This is proof-of-concept code for letting the physical core count affect
  // the interval into which we attempt to scale. For now, the code is Mac OS
  // specific, since that's the platform were we saw most problems.
  // TODO(torbjorng): Enhance SystemInfo to return this metric.

  mach_port_t mach_host = mach_host_self();
  host_basic_info hbi = {};
  mach_msg_type_number_t info_count = HOST_BASIC_INFO_COUNT;
  kern_return_t kr =
      host_info(mach_host, HOST_BASIC_INFO, reinterpret_cast<host_info_t>(&hbi),
                &info_count);
  mach_port_deallocate(mach_task_self(), mach_host);

  int n_physical_cores;
  if (kr != KERN_SUCCESS) {
    // If we couldn't get # of physical CPUs, don't panic. Assume we have 1.
    n_physical_cores = 1;
    RTC_LOG(LS_ERROR)
        << "Failed to determine number of physical cores, assuming 1";
  } else {
    n_physical_cores = hbi.physical_cpu;
    RTC_LOG(LS_INFO) << "Number of physical cores:" << n_physical_cores;
  }

  // Change init list default for few core systems. The assumption here is that
  // encoding, which we measure here, takes about 1/4 of the processing of a
  // two-way call. This is roughly true for x86 using both vp8 and vp9 without
  // hardware encoding. Since we don't affect the incoming stream here, we only
  // control about 1/2 of the total processing needs, but this is not taken into
  // account.
  if (n_physical_cores == 1)
    high_encode_usage_threshold_percent = 20;  // Roughly 1/4 of 100%.
  else if (n_physical_cores == 2)
    high_encode_usage_threshold_percent = 40;  // Roughly 1/4 of 200%.
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

  // Note that we make the interval 2x+epsilon wide, since libyuv scaling steps
  // are close to that (when squared). This wide interval makes sure that
  // scaling up or down does not jump all the way across the interval.
  low_encode_usage_threshold_percent =
      (high_encode_usage_threshold_percent - 1) / 2;
}

// Class for calculating the processing usage on the send-side (the average
// processing time of a frame divided by the average time difference between
// captured frames).
class OveruseFrameDetector::SendProcessingUsage {
 public:
  explicit SendProcessingUsage(const CpuOveruseOptions& options)
      : options_(options) {
    Reset();
  }
  virtual ~SendProcessingUsage() {}

  void Reset() {
    // Start in between the underuse and overuse threshold.
    load_estimate_ = (options_.low_encode_usage_threshold_percent +
                      options_.high_encode_usage_threshold_percent) /
                     200.0;
  }

  void AddSample(double encode_time, double diff_time) {
    RTC_CHECK_GE(diff_time, 0.0);

    // Use the filter update
    //
    // load <-- x/d (1-exp (-d/T)) + exp (-d/T) load
    //
    // where we must take care for small d, using the proper limit
    // (1 - exp(-d/tau)) / d = 1/tau - d/2tau^2 + O(d^2)
    double tau = (1e-3 * options_.filter_time_ms);
    double e = diff_time / tau;
    double c;
    if (e < 0.0001) {
      c = (1 - e / 2) / tau;
    } else {
      c = -expm1(-e) / diff_time;
    }
    load_estimate_ = c * encode_time + exp(-e) * load_estimate_;
  }

  virtual int Value() { return static_cast<int>(100.0 * load_estimate_ + 0.5); }

 private:
  const CpuOveruseOptions options_;
  double load_estimate_;
};

// Class used for manual testing of overuse, enabled via field trial flag.
class OveruseFrameDetector::OverdoseInjector
    : public OveruseFrameDetector::SendProcessingUsage {
 public:
  OverdoseInjector(const CpuOveruseOptions& options,
                   int64_t normal_period_ms,
                   int64_t overuse_period_ms,
                   int64_t underuse_period_ms)
      : OveruseFrameDetector::SendProcessingUsage(options),
        normal_period_ms_(normal_period_ms),
        overuse_period_ms_(overuse_period_ms),
        underuse_period_ms_(underuse_period_ms),
        state_(State::kNormal),
        last_toggling_ms_(-1) {
    RTC_DCHECK_GT(overuse_period_ms, 0);
    RTC_DCHECK_GT(normal_period_ms, 0);
    RTC_LOG(LS_INFO) << "Simulating overuse with intervals " << normal_period_ms
                     << "ms normal mode, " << overuse_period_ms
                     << "ms overuse mode.";
  }

  ~OverdoseInjector() override {}

  int Value() override {
    int64_t now_ms = rtc::TimeMillis();
    if (last_toggling_ms_ == -1) {
      last_toggling_ms_ = now_ms;

    } else {
      switch (state_) {
        case State::kNormal:
          if (now_ms > last_toggling_ms_ + normal_period_ms_) {
            state_ = State::kOveruse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU overuse.";
          }
          break;
        case State::kOveruse:
          if (now_ms > last_toggling_ms_ + overuse_period_ms_) {
            state_ = State::kUnderuse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU underuse.";
          }
          break;
        case State::kUnderuse:
          if (now_ms > last_toggling_ms_ + underuse_period_ms_) {
            state_ = State::kNormal;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Actual CPU overuse measurements in effect.";
          }
          break;
      }
    }

    rtc::Optional<int> overried_usage_value;
    switch (state_) {
      case State::kNormal:
        break;
      case State::kOveruse:
        overried_usage_value.emplace(250);
        break;
      case State::kUnderuse:
        overried_usage_value.emplace(5);
        break;
    }
    return overried_usage_value.value_or(SendProcessingUsage::Value());
  }

 private:
  const int64_t normal_period_ms_;
  const int64_t overuse_period_ms_;
  const int64_t underuse_period_ms_;
  enum class State { kNormal, kOveruse, kUnderuse } state_;
  int64_t last_toggling_ms_;
};

std::unique_ptr<OveruseFrameDetector::SendProcessingUsage>
OveruseFrameDetector::CreateSendProcessingUsage(
    const CpuOveruseOptions& options) {
  std::unique_ptr<SendProcessingUsage> instance;
  std::string toggling_interval =
      field_trial::FindFullName("WebRTC-ForceSimulatedOveruseIntervalMs");
  if (!toggling_interval.empty()) {
    int normal_period_ms = 0;
    int overuse_period_ms = 0;
    int underuse_period_ms = 0;
    if (sscanf(toggling_interval.c_str(), "%d-%d-%d", &normal_period_ms,
               &overuse_period_ms, &underuse_period_ms) == 3) {
      if (normal_period_ms > 0 && overuse_period_ms > 0 &&
          underuse_period_ms > 0) {
        instance.reset(new OverdoseInjector(
            options, normal_period_ms, overuse_period_ms, underuse_period_ms));
      } else {
        RTC_LOG(LS_WARNING)
            << "Invalid (non-positive) normal/overuse/underuse periods: "
            << normal_period_ms << " / " << overuse_period_ms << " / "
            << underuse_period_ms;
      }
    } else {
      RTC_LOG(LS_WARNING) << "Malformed toggling interval: "
                          << toggling_interval;
    }
  }

  if (!instance) {
    // No valid overuse simulation parameters set, use normal usage class.
    instance.reset(new SendProcessingUsage(options));
  }

  return instance;
}

class OveruseFrameDetector::CheckOveruseTask : public rtc::QueuedTask {
 public:
  explicit CheckOveruseTask(OveruseFrameDetector* overuse_detector)
      : overuse_detector_(overuse_detector) {
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kTimeToFirstCheckForOveruseMs);
  }

  void Stop() {
    RTC_CHECK(task_checker_.CalledSequentially());
    overuse_detector_ = nullptr;
  }

 private:
  bool Run() override {
    RTC_CHECK(task_checker_.CalledSequentially());
    if (!overuse_detector_)
      return true;  // This will make the task queue delete this task.
    overuse_detector_->CheckForOveruse();

    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kCheckForOveruseIntervalMs);
    // Return false to prevent this task from being deleted. Ownership has been
    // transferred to the task queue when PostDelayedTask was called.
    return false;
  }
  rtc::SequencedTaskChecker task_checker_;
  OveruseFrameDetector* overuse_detector_;
};

OveruseFrameDetector::OveruseFrameDetector(
    const CpuOveruseOptions& options,
    AdaptationObserverInterface* observer,
    EncodedFrameObserver* encoder_timing,
    CpuOveruseMetricsObserver* metrics_observer)
    : check_overuse_task_(nullptr),
      options_(options),
      observer_(observer),
      encoder_timing_(encoder_timing),
      metrics_observer_(metrics_observer),
      num_process_times_(0),
      // TODO(nisse): Use rtc::Optional
      last_capture_time_us_(-1),
      last_processed_capture_time_us_(-1),
      num_pixels_(0),
      last_overuse_time_ms_(-1),
      checks_above_threshold_(0),
      num_overuse_detections_(0),
      last_rampup_time_ms_(-1),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      usage_(CreateSendProcessingUsage(options)) {
  task_checker_.Detach();
}

OveruseFrameDetector::~OveruseFrameDetector() {
  RTC_DCHECK(!check_overuse_task_) << "StopCheckForOverUse must be called.";
}

void OveruseFrameDetector::StartCheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  RTC_DCHECK(!check_overuse_task_);
  check_overuse_task_ = new CheckOveruseTask(this);
}
void OveruseFrameDetector::StopCheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  check_overuse_task_->Stop();
  check_overuse_task_ = nullptr;
}

void OveruseFrameDetector::EncodedFrameTimeMeasured(int encode_duration_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (!metrics_)
    metrics_ = rtc::Optional<CpuOveruseMetrics>(CpuOveruseMetrics());
  metrics_->encode_usage_percent = usage_->Value();

  metrics_observer_->OnEncodedFrameTimeMeasured(encode_duration_ms, *metrics_);
}

bool OveruseFrameDetector::FrameSizeChanged(int num_pixels) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (num_pixels != num_pixels_) {
    return true;
  }
  return false;
}

bool OveruseFrameDetector::FrameTimeoutDetected(int64_t now_us) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (last_capture_time_us_ == -1)
    return false;
  return (now_us - last_capture_time_us_) >
      options_.frame_timeout_interval_ms * rtc::kNumMicrosecsPerMillisec;
}

void OveruseFrameDetector::ResetAll() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  usage_->Reset();
  last_capture_time_us_ = -1;
  last_processed_capture_time_us_ = -1;
  num_process_times_ = 0;
  metrics_ = rtc::Optional<CpuOveruseMetrics>();
}

void OveruseFrameDetector::FrameCaptured(int width, int height) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);

  if (FrameSizeChanged(width * height)) {
    ResetAll();
    num_pixels_ = width * height;
  }
}

void OveruseFrameDetector::FrameEncoded(int64_t capture_time_us,
                                        int64_t encode_duration_us) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (FrameTimeoutDetected(capture_time_us)) {
    ResetAll();
  } else if (last_capture_time_us_ != -1) {
    usage_->AddSample(1e-6 * encode_duration_us,
                      1e-6 * (capture_time_us - last_capture_time_us_));
  }
  last_capture_time_us_ = capture_time_us;
  EncodedFrameTimeMeasured(encode_duration_us / rtc::kNumMicrosecsPerMillisec);

  if (encoder_timing_) {
    // TODO(nisse): Update encoder_timing_ to also use us units.
    encoder_timing_->OnEncodeTiming(
        capture_time_us / rtc::kNumMicrosecsPerMillisec,
        encode_duration_us / rtc::kNumMicrosecsPerMillisec);
  }
}

void OveruseFrameDetector::CheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  ++num_process_times_;
  if (num_process_times_ <= options_.min_process_count || !metrics_)
    return;

  int64_t now_ms = rtc::TimeMillis();

  if (IsOverusing(*metrics_)) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ms_ > last_overuse_time_ms_;
    if (check_for_backoff) {
      if (now_ms - last_rampup_time_ms_ < kStandardRampUpDelayMs ||
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

    last_overuse_time_ms_ = now_ms;
    in_quick_rampup_ = false;
    checks_above_threshold_ = 0;
    ++num_overuse_detections_;

    if (observer_)
      observer_->AdaptDown(kScaleReasonCpu);
  } else if (IsUnderusing(*metrics_, now_ms)) {
    last_rampup_time_ms_ = now_ms;
    in_quick_rampup_ = true;

    if (observer_)
      observer_->AdaptUp(kScaleReasonCpu);
  }

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;

  RTC_LOG(LS_VERBOSE) << " Frame stats: "
                      << " encode usage " << metrics_->encode_usage_percent
                      << " overuse detections " << num_overuse_detections_
                      << " rampup delay " << rampup_delay;
}

bool OveruseFrameDetector::IsOverusing(const CpuOveruseMetrics& metrics) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);

  if (metrics.encode_usage_percent >=
      options_.high_encode_usage_threshold_percent) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(const CpuOveruseMetrics& metrics,
                                        int64_t time_now) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ms_ + delay)
    return false;

  return metrics.encode_usage_percent <
         options_.low_encode_usage_threshold_percent;
}
}  // namespace webrtc
