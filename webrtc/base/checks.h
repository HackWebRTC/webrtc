/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This module contains some basic debugging facilities.
// Originally comes from shared/commandlineflags/checks.h

#ifndef WEBRTC_BASE_CHECKS_H_
#define WEBRTC_BASE_CHECKS_H_

#include <string.h>

namespace rtc {

// Prints an error message to stderr and aborts execution.
void Fatal(const char* file, int line, const char* format, ...);

}  // namespace rtc

// The UNREACHABLE macro is very useful during development.
#define UNREACHABLE()                                   \
  rtc::Fatal(__FILE__, __LINE__, "unreachable code")

#endif  // WEBRTC_BASE_CHECKS_H_
