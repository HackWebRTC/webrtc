/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FORMAT_MACROS_H_
#define RTC_BASE_FORMAT_MACROS_H_

// This file defines the format macros for some integer types and is derived
// from Chromium's base/format_macros.h.

// To print a 64-bit value in a portable way:
//   int64_t value;
//   printf("xyz:%" PRId64, value);
// The "d" in the macro corresponds to %d; you can also use PRIu64 etc.
//
// To print a size_t value in a portable way:
//   size_t size;
//   printf("xyz: %" RTC_PRIuS, size);
// The "u" in the macro corresponds to %u, and S is for "size".

#if defined(WEBRTC_POSIX)

#if (defined(_INTTYPES_H) || defined(_INTTYPES_H_)) && !defined(PRId64)
#error "inttypes.h has already been included before this header file, but "
#error "without __STDC_FORMAT_MACROS defined."
#endif

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

#include "rtc_base/system/arch.h"

#define RTC_PRIuS "zu"

#else  // WEBRTC_WIN

#include <inttypes.h>

// These are being defined without the RTC_ prefix because this is just filling
// the holes from what's supposed to be already present as part of the C
// standard, but missing on older MSVC versions.

#if !defined(PRId64)
#define PRId64 "I64d"
#endif

#if !defined(PRIu64)
#define PRIu64 "I64u"
#endif

#if !defined(PRIx64)
#define PRIx64 "I64x"
#endif

// PRI*64 were added in MSVC 2013, while "%zu" is supported since MSVC 2015
// (so needs to be special-cased to "%Iu" instead).

#define RTC_PRIuS "Iu"

#endif

#endif  // RTC_BASE_FORMAT_MACROS_H_
