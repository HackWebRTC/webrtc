/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TICK_TIME_WRAPPER_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TICK_TIME_WRAPPER_H_

#include "system_wrappers/interface/tick_util.h"

namespace webrtc {

class TickTimeInterface : public TickTime {
 public:
  // Current time in the tick domain.
  virtual TickTime Now() const { return TickTime::Now(); }

  // Now in the time domain in ms.
  virtual int64_t MillisecondTimestamp() const {
    return TickTime::MillisecondTimestamp();
  }

  // Now in the time domain in us.
  virtual int64_t MicrosecondTimestamp() const {
    return TickTime::MicrosecondTimestamp();
  }
};

}  // namespace

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TICK_TIME_WRAPPER_H_
