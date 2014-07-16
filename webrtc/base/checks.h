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

namespace rtc {

// Prints an error message to stderr and aborts execution.
void Fatal(const char* file, int line, const char* format, ...);

}  // namespace rtc

// Trigger a fatal error (which aborts the process and prints an error
// message). FATAL_ERROR_IF may seem a lot like assert, but there's a crucial
// difference: it's always "on". This means that it can be used to check for
// regular errors that could actually happen, not just programming errors that
// supposedly can't happen---but triggering a fatal error will kill the process
// in an ugly way, so it's not suitable for catching errors that might happen
// in production.
#define FATAL_ERROR(msg) do { rtc::Fatal(__FILE__, __LINE__, msg); } while (0)
#define FATAL_ERROR_IF(x) do { if (x) FATAL_ERROR("check failed"); } while (0)

// The UNREACHABLE macro is very useful during development.
#define UNREACHABLE() FATAL_ERROR("unreachable code")

#endif  // WEBRTC_BASE_CHECKS_H_
