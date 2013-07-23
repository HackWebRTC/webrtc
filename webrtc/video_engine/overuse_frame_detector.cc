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

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

// TODO(mflodman) Test different thresholds.
const int64_t kProcessIntervalMs = 2000;
const int kOveruseHistoryMs = 5000;
const float kMinEncodeRatio = 29 / 30.0f;
const int kMinCallbackDeltaMs = 30000;

OveruseFrameDetector::OveruseFrameDetector(Clock* clock)
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(NULL),
      clock_(clock),
      last_process_time_(clock->TimeInMilliseconds()),
      last_callback_time_(clock->TimeInMilliseconds()) {
}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  observer_ = observer;
}

void OveruseFrameDetector::CapturedFrame() {
  CriticalSectionScoped cs(crit_.get());
  CleanOldSamples();
  capture_times_.push_back(clock_->TimeInMilliseconds());
}

void OveruseFrameDetector::EncodedFrame() {
  CriticalSectionScoped cs(crit_.get());
  encode_times_.push_back(clock_->TimeInMilliseconds());
}

int32_t OveruseFrameDetector::TimeUntilNextProcess() {
  return last_process_time_ + kProcessIntervalMs - clock_->TimeInMilliseconds();
}

int32_t OveruseFrameDetector::Process() {
  CriticalSectionScoped cs(crit_.get());
  int64_t now = clock_->TimeInMilliseconds();
  if (now < last_process_time_ + kProcessIntervalMs)
    return 0;

  last_process_time_ = now;
  if (!observer_ || encode_times_.size() == 0 || capture_times_.size() == 0)
    return 0;

  CleanOldSamples();
  if (encode_times_.front() > now - kOveruseHistoryMs / 2) {
    return 0;
  }
  float encode_ratio = encode_times_.size() /
      static_cast<float>(capture_times_.size());
  if (encode_ratio < kMinEncodeRatio) {
    observer_->OveruseDetected();
    capture_times_.clear();
    encode_times_.clear();
    last_callback_time_ = now;
  } else if (last_callback_time_ < now - kMinCallbackDeltaMs) {
    // TODO(mflodman) This should only be triggered if we have a good reason to
    // believe we can increase the resolution again.
    observer_->NormalUsage();
    last_callback_time_ = now;
    capture_times_.clear();
    encode_times_.clear();
  }
  return 0;
}

void OveruseFrameDetector::CleanOldSamples() {
  int64_t time_now = clock_->TimeInMilliseconds();
  while (!capture_times_.empty() &&
         capture_times_.front() < time_now - kOveruseHistoryMs) {
    capture_times_.pop_front();
  }
  while (!encode_times_.empty() &&
         encode_times_.front() < time_now - kOveruseHistoryMs) {
    encode_times_.pop_front();
  }
}
}  // namespace webrtc
