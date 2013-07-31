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

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

// TODO(mflodman) Test different values for all of these to trigger correctly,
// avoid fluctuations etc.

namespace {
// Interval for 'Process' to be called.
const int64_t kProcessIntervalMs = 2000;

// Duration capture and encode samples are valid.
const int kOveruseHistoryMs = 5000;

// The minimum history to trigger an overuse or underuse.
const int64_t kMinValidHistoryMs = kOveruseHistoryMs / 2;

// Encode / capture ratio to decide an overuse.
const float kMinEncodeRatio = 29 / 30.0f;

// Minimum time between two callbacks.
const int kMinCallbackDeltaMs = 30000;

// Safety margin between encode time for different resolutions to decide if we
// can trigger an underuse callback.
// TODO(mflodman): This should be improved, e.g. test time per pixel?
const float kIncreaseThreshold = 1.5f;
}  // namespace

OveruseFrameDetector::OveruseFrameDetector(Clock* clock)
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      clock_(clock),
      last_process_time_(clock->TimeInMilliseconds()),
      last_callback_time_(clock->TimeInMilliseconds()),
      underuse_encode_timing_enabled_(false),
      num_pixels_(0),
      max_num_pixels_(0) {
}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  observer_ = observer;
}

void OveruseFrameDetector::set_underuse_encode_timing_enabled(bool enable) {
  CriticalSectionScoped cs(crit_.get());
  underuse_encode_timing_enabled_ = enable;
}

void OveruseFrameDetector::FrameCaptured() {
  CriticalSectionScoped cs(crit_.get());
  capture_times_.push_back(clock_->TimeInMilliseconds());
}

void OveruseFrameDetector::FrameEncoded(int64_t encode_time, size_t width,
                                        size_t height) {
  assert(encode_time >= 0);
  CriticalSectionScoped cs(crit_.get());
  // The frame is disregarded in case of a reset, to startup in a fresh state.
  if (MaybeResetResolution(width, height))
    return;

  encode_times_.push_back(std::make_pair(clock_->TimeInMilliseconds(),
                                         encode_time));
}

int32_t OveruseFrameDetector::TimeUntilNextProcess() {
  CriticalSectionScoped cs(crit_.get());
  return last_process_time_ + kProcessIntervalMs - clock_->TimeInMilliseconds();
}

int32_t OveruseFrameDetector::Process() {
  CriticalSectionScoped cs(crit_.get());
  int64_t now = clock_->TimeInMilliseconds();
  if (now < last_process_time_ + kProcessIntervalMs)
    return 0;

  last_process_time_ = now;
  RemoveOldSamples();

  // Don't trigger an overuse unless we've encoded at least one frame.
  if (!observer_ || encode_times_.empty() || capture_times_.empty())
    return 0;

  // At least half the maximum history should be filled before we trigger an
  // overuse.
  // TODO(mflodman) Shall the time difference between the first and the last
  // sample be checked instead?
  if (encode_times_.front().first > now - kMinValidHistoryMs) {
    return 0;
  }

  if (IsOverusing()) {
    // Overuse detected.
    // Remember the average encode time for this overuse, as a help to trigger
    // normal usage.
    encode_overuse_times_[num_pixels_] = CalculateAverageEncodeTime();
    RemoveAllSamples();
    observer_->OveruseDetected();
    last_callback_time_ = now;
  } else if (IsUnderusing(now)) {
    RemoveAllSamples();
    observer_->NormalUsage();
    last_callback_time_ = now;
  }
  return 0;
}

void OveruseFrameDetector::RemoveOldSamples() {
  int64_t time_now = clock_->TimeInMilliseconds();
  while (!capture_times_.empty() &&
         capture_times_.front() < time_now - kOveruseHistoryMs) {
    capture_times_.pop_front();
  }
  while (!encode_times_.empty() &&
         encode_times_.front().first < time_now - kOveruseHistoryMs) {
    encode_times_.pop_front();
  }
}

void OveruseFrameDetector::RemoveAllSamples() {
  capture_times_.clear();
  encode_times_.clear();
}

int64_t OveruseFrameDetector::CalculateAverageEncodeTime() const {
  if (encode_times_.empty())
    return 0;

  int64_t total_encode_time = 0;
  for (std::list<EncodeTime>::const_iterator it = encode_times_.begin();
       it != encode_times_.end(); ++it) {
    total_encode_time += it->second;
  }
  return total_encode_time / encode_times_.size();
}

bool OveruseFrameDetector::MaybeResetResolution(size_t width, size_t height) {
  int num_pixels = width * height;
  if (num_pixels == num_pixels_)
    return false;

  RemoveAllSamples();
  num_pixels_ = num_pixels;
  if (num_pixels > max_num_pixels_)
    max_num_pixels_ = num_pixels;

  return true;
}

bool OveruseFrameDetector::IsOverusing() {
  if (encode_times_.empty())
    return false;

  float encode_ratio = encode_times_.size() /
      static_cast<float>(capture_times_.size());
  return encode_ratio < kMinEncodeRatio;
}

bool OveruseFrameDetector::IsUnderusing(int64_t time_now) {
  if (time_now < last_callback_time_ + kMinCallbackDeltaMs ||
      num_pixels_ >= max_num_pixels_) {
    return false;
  }
  bool underusing = true;
  if (underuse_encode_timing_enabled_) {
    int prev_overuse_encode_time = 0;
    for (std::map<int, int64_t>::reverse_iterator rit =
             encode_overuse_times_.rbegin();
         rit != encode_overuse_times_.rend() && rit->first > num_pixels_;
         ++rit) {
      prev_overuse_encode_time = rit->second;
    }
    // TODO(mflodman): This might happen now if the resolution is decreased
    // by the user before an overuse has been triggered.
    assert(prev_overuse_encode_time > 0);

    // TODO(mflodman) Use some other way to guess if an increased resolution
    // might work or not, e.g. encode time per pixel?
    if (CalculateAverageEncodeTime() * kIncreaseThreshold >
        prev_overuse_encode_time) {
      underusing = false;
    }
  }
  return underusing;
}
}  // namespace webrtc
