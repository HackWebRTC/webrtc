/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_OVERUSE_FRAME_DETECTOR_H_
#define VIDEO_OVERUSE_FRAME_DETECTOR_H_

#include <list>
#include <memory>

#include "api/optional.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class EncodedFrameObserver;
class VideoFrame;

struct CpuOveruseOptions {
  CpuOveruseOptions();

  int low_encode_usage_threshold_percent;  // Threshold for triggering underuse.
  int high_encode_usage_threshold_percent;  // Threshold for triggering overuse.
  // General settings.
  int frame_timeout_interval_ms;  // The maximum allowed interval between two
                                  // frames before resetting estimations.
  int min_process_count;  // The number of initial process times required before
                          // triggering an overuse/underuse.
  int high_threshold_consecutive_count;  // The number of consecutive checks
                                         // above the high threshold before
                                         // triggering an overuse.
  int filter_time_ms;                    // Time constant for averaging
};

struct CpuOveruseMetrics {
  CpuOveruseMetrics() : encode_usage_percent(-1) {}

  int encode_usage_percent;  // Average encode time divided by the average time
                             // difference between incoming captured frames.
};

class CpuOveruseMetricsObserver {
 public:
  virtual ~CpuOveruseMetricsObserver() {}
  virtual void OnEncodedFrameTimeMeasured(int encode_duration_ms,
                                          const CpuOveruseMetrics& metrics) = 0;
};

// Use to detect system overuse based on the send-side processing time of
// incoming frames. All methods must be called on a single task queue but it can
// be created and destroyed on an arbitrary thread.
// OveruseFrameDetector::StartCheckForOveruse  must be called to periodically
// check for overuse.
class OveruseFrameDetector {
 public:
  OveruseFrameDetector(const CpuOveruseOptions& options,
                       AdaptationObserverInterface* overuse_observer,
                       EncodedFrameObserver* encoder_timing_,
                       CpuOveruseMetricsObserver* metrics_observer);
  virtual ~OveruseFrameDetector();

  // Start to periodically check for overuse.
  void StartCheckForOveruse();

  // StopCheckForOveruse must be called before destruction if
  // StartCheckForOveruse has been called.
  void StopCheckForOveruse();

  // Called for each captured frame.
  void FrameCaptured(int width, int height);

  // Called for each encoded frame.
  void FrameEncoded(int64_t capture_time_us, int64_t encode_duration_us);

 protected:
  void CheckForOveruse();  // Protected for test purposes.

 private:
  class OverdoseInjector;
  class SendProcessingUsage;
  class CheckOveruseTask;

  void EncodedFrameTimeMeasured(int encode_duration_ms);
  bool IsOverusing(const CpuOveruseMetrics& metrics);
  bool IsUnderusing(const CpuOveruseMetrics& metrics, int64_t time_now);

  bool FrameTimeoutDetected(int64_t now) const;
  bool FrameSizeChanged(int num_pixels) const;

  void ResetAll();

  static std::unique_ptr<SendProcessingUsage> CreateSendProcessingUsage(
      const CpuOveruseOptions& options);

  rtc::SequencedTaskChecker task_checker_;
  // Owned by the task queue from where StartCheckForOveruse is called.
  CheckOveruseTask* check_overuse_task_;

  const CpuOveruseOptions options_;

  // Observer getting overuse reports.
  AdaptationObserverInterface* const observer_;
  EncodedFrameObserver* const encoder_timing_;

  // Stats metrics.
  CpuOveruseMetricsObserver* const metrics_observer_;
  rtc::Optional<CpuOveruseMetrics> metrics_ RTC_GUARDED_BY(task_checker_);

  int64_t num_process_times_ RTC_GUARDED_BY(task_checker_);

  int64_t last_capture_time_us_ RTC_GUARDED_BY(task_checker_);
  int64_t last_processed_capture_time_us_ RTC_GUARDED_BY(task_checker_);

  // Number of pixels of last captured frame.
  int num_pixels_ RTC_GUARDED_BY(task_checker_);
  int64_t last_overuse_time_ms_ RTC_GUARDED_BY(task_checker_);
  int checks_above_threshold_ RTC_GUARDED_BY(task_checker_);
  int num_overuse_detections_ RTC_GUARDED_BY(task_checker_);
  int64_t last_rampup_time_ms_ RTC_GUARDED_BY(task_checker_);
  bool in_quick_rampup_ RTC_GUARDED_BY(task_checker_);
  int current_rampup_delay_ms_ RTC_GUARDED_BY(task_checker_);

  // TODO(asapersson): Can these be regular members (avoid separate heap
  // allocs)?
  const std::unique_ptr<SendProcessingUsage> usage_
      RTC_GUARDED_BY(task_checker_);

  RTC_DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_H_
