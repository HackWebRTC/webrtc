/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
#define WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

class Clock;
class CpuOveruseObserver;
class CriticalSectionWrapper;
class VCMExpFilter;

// TODO(pbos): Move this somewhere appropriate.
class Statistics {
 public:
  Statistics();

  void AddSample(float sample_ms);
  void Reset();
  void SetOptions(const CpuOveruseOptions& options);

  float Mean() const;
  float StdDev() const;
  uint64_t Count() const;

 private:
  float InitialMean() const;
  float InitialVariance() const;

  float sum_;
  uint64_t count_;
  CpuOveruseOptions options_;
  scoped_ptr<VCMExpFilter> filtered_samples_;
  scoped_ptr<VCMExpFilter> filtered_variance_;
};

// Use to detect system overuse based on jitter in incoming frames.
class OveruseFrameDetector : public Module {
 public:
  explicit OveruseFrameDetector(Clock* clock);
  ~OveruseFrameDetector();

  // Registers an observer receiving overuse and underuse callbacks. Set
  // 'observer' to NULL to disable callbacks.
  void SetObserver(CpuOveruseObserver* observer);

  // Sets options for overuse detection.
  void SetOptions(const CpuOveruseOptions& options);

  // Called for each captured frame.
  void FrameCaptured(int width, int height);

  // Called when the processing of a captured frame is started.
  void FrameProcessingStarted();

  // Called for each encoded frame.
  void FrameEncoded(int encode_time_ms);

  // Accessors.
  // The estimated jitter based on incoming captured frames.
  int CaptureJitterMs() const;

  // Running average of reported encode time (FrameEncoded()).
  // Only used for stats.
  int AvgEncodeTimeMs() const;

  // The average encode time divided by the average time difference between
  // incoming captured frames.
  // This variable is currently only used for statistics.
  int EncodeUsagePercent() const;

  // The current time delay between an incoming captured frame (FrameCaptured())
  // until the frame is being processed (FrameProcessingStarted()).
  // (Note: if a new frame is received before an old frame has been processed,
  // the old frame is skipped).
  // The delay is returned as the delay in ms per second.
  // This variable is currently only used for statistics.
  int AvgCaptureQueueDelayMsPerS() const;
  int CaptureQueueDelayMsPerS() const;

  // Implements Module.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

 private:
  class EncodeTimeAvg;
  class EncodeUsage;
  class CaptureQueueDelay;

  bool IsOverusing();
  bool IsUnderusing(int64_t time_now);

  bool FrameTimeoutDetected(int64_t now) const;
  bool FrameSizeChanged(int num_pixels) const;

  void ResetAll(int num_pixels);

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_;

  CpuOveruseOptions options_;

  Clock* clock_;
  int64_t next_process_time_;
  int64_t num_process_times_;

  Statistics capture_deltas_;
  int64_t last_capture_time_;

  int64_t last_overuse_time_;
  int checks_above_threshold_;

  int64_t last_rampup_time_;
  bool in_quick_rampup_;
  int current_rampup_delay_ms_;

  // Number of pixels of last captured frame.
  int num_pixels_;

  int64_t last_encode_sample_ms_;
  scoped_ptr<EncodeTimeAvg> encode_time_;
  scoped_ptr<EncodeUsage> encode_usage_;

  scoped_ptr<CaptureQueueDelay> capture_queue_delay_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
