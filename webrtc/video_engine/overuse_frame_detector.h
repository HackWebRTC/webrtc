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

#include <list>
#include <map>
#include <utility>

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;
class CpuOveruseObserver;

// Use to detect system overuse based on the number of captured frames vs the
// number of encoded frames.
class OveruseFrameDetector : public Module {
 public:
  explicit OveruseFrameDetector(Clock* clock);
  ~OveruseFrameDetector();

  // Registers an observer receiving overuse and underuse callbacks. Set
  // 'observer' to NULL to disable callbacks.
  void SetObserver(CpuOveruseObserver* observer);

  // TODO(mflodman): Move to another API?
  // Enables usage of encode time to trigger normal usage after an overuse,
  // default false.
  void set_underuse_encode_timing_enabled(bool enable);

  // Called for each captured frame.
  void FrameCaptured();

  // Called for every encoded frame.
  void FrameEncoded(int64_t encode_time, size_t width, size_t height);

  // Implements Module.
  virtual int32_t TimeUntilNextProcess();
  virtual int32_t Process();

 private:
  // All private functions are assumed to be critical section protected.
  // Clear samples older than the overuse history.
  void RemoveOldSamples();
  // Clears the entire history, including samples still affecting the
  // calculations.
  void RemoveAllSamples();
  int64_t CalculateAverageEncodeTime() const;
  // Returns true and resets calculations and history if a new resolution is
  // discovered, false otherwise.
  bool MaybeResetResolution(size_t width, size_t height);

  bool IsOverusing();
  bool IsUnderusing(int64_t time_now);

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_;

  Clock* clock_;
  int64_t last_process_time_;
  int64_t last_callback_time_;

  // Sorted list of times captured frames were delivered, oldest frame first.
  std::list<int64_t> capture_times_;
  // <Encode report time, time spent encoding the frame>.
  typedef std::pair<int64_t, int64_t> EncodeTime;
  // Sorted list with oldest frame first.
  std::list<EncodeTime> encode_times_;

  // True if encode time should be considered to trigger an underuse.
  bool underuse_encode_timing_enabled_;
  // Number of pixels in the currently encoded resolution.
  int num_pixels_;
  // Maximum resolution encoded.
  int max_num_pixels_;
  // <number of pixels, average encode time triggering an overuse>.
  std::map<int, int64_t> encode_overuse_times_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
