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

  void SetObserver(CpuOveruseObserver* observer);

  // Called for each new captured frame.
  void CapturedFrame();

  // Called for every encoded frame.
  void EncodedFrame();

  // Implements Module.
  virtual int32_t TimeUntilNextProcess();
  virtual int32_t Process();

 private:
  void CleanOldSamples();

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_;

  Clock* clock_;
  int64_t last_process_time_;
  int64_t last_callback_time_;

  // Capture time for frames.
  std::list<int64_t> capture_times_;

  // Start encode time for a frame.
  std::list<int64_t> encode_times_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
