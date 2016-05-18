/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/thread_annotations.h"

namespace webrtc {

class Clock;
class VCMJitterEstimator;
class VCMTiming;

namespace video_coding {

class FrameObject;

class FrameBuffer {
 public:
  FrameBuffer(Clock* clock,
              VCMJitterEstimator* jitter_estimator,
              const VCMTiming* timing);

  // Insert a frame into the frame buffer.
  void InsertFrame(std::unique_ptr<FrameObject> frame);

  // Get the next frame for decoding. Will return at latest after
  // |max_wait_time_ms|, with either a managed FrameObject or an empty
  // unique ptr if there is no available frame for decoding.
  std::unique_ptr<FrameObject> NextFrame(int64_t max_wait_time_ms);

 private:
  // FrameKey is a pair of (picture id, spatial layer).
  using FrameKey = std::pair<uint16_t, uint8_t>;

  // Comparator used to sort frames, first on their picture id, and second
  // on their spatial layer.
  struct FrameComp {
    bool operator()(const FrameKey& f1, const FrameKey& f2) const;
  };

  // Determines whether a frame is continuous.
  bool IsContinuous(const FrameObject& frame) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Keep track of decoded frames.
  std::set<FrameKey, FrameComp> decoded_frames_ GUARDED_BY(crit_);

  // The actual buffer that holds the FrameObjects.
  std::map<FrameKey, std::unique_ptr<FrameObject>, FrameComp> frames_
      GUARDED_BY(crit_);

  rtc::CriticalSection crit_;
  Clock* const clock_;
  rtc::Event frame_inserted_event_;
  VCMJitterEstimator* const jitter_estimator_;
  const VCMTiming* const timing_;
  int newest_picture_id_ GUARDED_BY(crit_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(FrameBuffer);
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_
