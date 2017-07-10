/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//   LOG(...) an ostream target that can be used to send formatted
// output to a variety of logging targets, such as debugger console, stderr,
// or any LogSink.
//   The severity level passed as the first argument to the LOGging
// functions is used as a filter, to limit the verbosity of the logging.
//   Static members of LogMessage documented below are used to control the
// verbosity and target of the output.
//   There are several variations on the LOG macro which facilitate logging
// of common error conditions, detailed below.

// LOG(sev) logs the given stream at severity "sev", which must be a
//     compile-time constant of the LoggingSeverity type, without the namespace
//     prefix.
// LOG_V(sev) Like LOG(), but sev is a run-time variable of the LoggingSeverity
//     type (basically, it just doesn't prepend the namespace).
// LOG_F(sev) Like LOG(), but includes the name of the current function.
// LOG_T(sev) Like LOG(), but includes the this pointer.
// LOG_T_F(sev) Like LOG_F(), but includes the this pointer.
// LOG_GLE(M)(sev [, mod]) attempt to add a string description of the
//     HRESULT returned by GetLastError.  The "M" variant allows searching of a
//     DLL's string table for the error description.
// LOG_ERRNO(sev) attempts to add a string description of an errno-derived
//     error. errno and associated facilities exist on both Windows and POSIX,
//     but on Windows they only apply to the C/C++ runtime.
// LOG_ERR(sev) is an alias for the platform's normal error system, i.e. _GLE on
//     Windows and _ERRNO on POSIX.
// (The above three also all have _EX versions that let you specify the error
// code, rather than using the last one.)
// LOG_E(sev, ctx, err, ...) logs a detailed error interpreted using the
//     specified context.
// LOG_CHECK_LEVEL(sev) (and LOG_CHECK_LEVEL_V(sev)) can be used as a test
//     before performing expensive or sensitive operations whose sole purpose is
//     to output logging data at the desired level.
// Lastly, PLOG(sev, err) is an alias for LOG_ERR_EX.

#ifndef WEBRTC_BASE_LOGGING_H_
#define WEBRTC_BASE_LOGGING_H_


// This header is deprecated and is just left here temporarily during
// refactoring. See https://bugs.webrtc.org/7634 for more details.
#include "webrtc/rtc_base/logging.h"

#endif  // WEBRTC_BASE_LOGGING_H_
