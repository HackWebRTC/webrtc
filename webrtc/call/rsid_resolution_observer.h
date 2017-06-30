/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_RSID_RESOLUTION_OBSERVER_H_
#define WEBRTC_CALL_RSID_RESOLUTION_OBSERVER_H_

#include <string>

#include "webrtc/rtc_base/basictypes.h"

namespace webrtc {

// One RSID can be associated with one, and only one, SSRC, throughout a call.
// The resolution might either happen during call setup, or during the call.
class RsidResolutionObserver {
 public:
  virtual ~RsidResolutionObserver() = default;

  virtual void OnRsidResolved(const std::string& rsid, uint32_t ssrc) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RSID_RESOLUTION_OBSERVER_H_
