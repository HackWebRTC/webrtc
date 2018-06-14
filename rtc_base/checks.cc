/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Most of this was borrowed (with minor modifications) from V8's and Chromium's
// src/base/logging.cc.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#if defined(WEBRTC_ANDROID)
#define RTC_LOG_TAG_ANDROID "rtc"
#include <android/log.h>  // NOLINT
#endif

#if defined(WEBRTC_WIN)
#include <windows.h>
#endif

#if defined(WEBRTC_WIN)
#define LAST_SYSTEM_ERROR (::GetLastError())
#elif defined(__native_client__) && __native_client__
#define LAST_SYSTEM_ERROR (0)
#elif defined(WEBRTC_POSIX)
#include <errno.h>
#define LAST_SYSTEM_ERROR (errno)
#endif  // WEBRTC_WIN

#include "rtc_base/checks.h"

namespace rtc {

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);
#endif
namespace webrtc_checks_impl {
RTC_NORETURN void FatalLog(const char* file,
                           int line,
                           const char* message,
                           const CheckArgType* fmt,
                           ...) {
  va_list args;
  va_start(args, fmt);

  std::ostringstream ss;  // no-presubmit-check TODO(webrtc:8982)
  ss << "\n\n#\n# Fatal error in: " << file << ", line " << line
     << "\n# last system error: " << LAST_SYSTEM_ERROR
     << "\n# Check failed: " << message << "\n# ";

  for (; *fmt != CheckArgType::kEnd; ++fmt) {
    switch (*fmt) {
      case CheckArgType::kInt:
        ss << va_arg(args, int);
        break;
      case CheckArgType::kLong:
        ss << va_arg(args, long);
        break;
      case CheckArgType::kLongLong:
        ss << va_arg(args, long long);
        break;
      case CheckArgType::kUInt:
        ss << va_arg(args, unsigned);
        break;
      case CheckArgType::kULong:
        ss << va_arg(args, unsigned long);
        break;
      case CheckArgType::kULongLong:
        ss << va_arg(args, unsigned long long);
        break;
      case CheckArgType::kDouble:
        ss << va_arg(args, double);
        break;
      case CheckArgType::kLongDouble:
        ss << va_arg(args, long double);
        break;
      case CheckArgType::kCharP:
        ss << va_arg(args, const char*);
        break;
      case CheckArgType::kStdString:
        ss << *va_arg(args, const std::string*);
        break;
      case CheckArgType::kVoidP:
        ss << va_arg(args, const void*);
        break;
      default:
        ss << "[Invalid CheckArgType:" << static_cast<int8_t>(*fmt) << "]";
        goto processing_loop_end;
    }
  }
processing_loop_end:
  va_end(args);

  std::string s = ss.str();
  const char* output = s.c_str();

#if defined(WEBRTC_ANDROID)
  __android_log_print(ANDROID_LOG_ERROR, RTC_LOG_TAG_ANDROID, "%s\n", output);
#endif

  fflush(stdout);
  fprintf(stderr, "%s", output);
  fflush(stderr);
  abort();
}

}  // namespace webrtc_checks_impl
}  // namespace rtc

// Function to call from the C version of the RTC_CHECK and RTC_DCHECK macros.
RTC_NORETURN void rtc_FatalMessage(const char* file, int line,
                                   const char* msg) {
  static constexpr rtc::webrtc_checks_impl::CheckArgType t[] = {
      rtc::webrtc_checks_impl::CheckArgType::kEnd};
  FatalLog(file, line, msg, t);
}
