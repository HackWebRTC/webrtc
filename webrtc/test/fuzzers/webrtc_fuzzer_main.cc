/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/base/logging.h"

// This file is intended to provide a common interface for fuzzing functions, so
// whether we're running fuzzing under libFuzzer or DrFuzz the webrtc functions
// can remain the same.
// TODO(pbos): Implement FuzzOneInput() for more than one platform (currently
// libFuzzer).

namespace webrtc {
extern void FuzzOneInput(const uint8_t* data, size_t size);
}  // namespace webrtc

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  // TODO(pbos): Figure out whether this can be moved to common startup code and
  // not be done per-input.
  // Remove default logging to prevent huge slowdowns.
  rtc::LogMessage::LogToDebug(rtc::LS_NONE);
  webrtc::FuzzOneInput(data, size);
  return 0;
}
