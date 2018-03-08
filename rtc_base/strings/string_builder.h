/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGS_STRING_BUILDER_H_
#define RTC_BASE_STRINGS_STRING_BUILDER_H_

#include <cstdio>
#include <cstring>
#include <string>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/stringutils.h"

namespace rtc {

// This is a minimalistic string builder class meant to cover the most cases of
// when you might otherwise be tempted to use a stringstream (discouraged for
// anything except logging). It uses a fixed-size buffer provided by the caller
// and concatenates strings and numbers into it, allowing the results to be
// read via |str()|.
class SimpleStringBuilder {
 public:
  explicit SimpleStringBuilder(rtc::ArrayView<char> buffer);
  SimpleStringBuilder(const SimpleStringBuilder&) = delete;
  SimpleStringBuilder& operator=(const SimpleStringBuilder&) = delete;

  SimpleStringBuilder& operator<<(const char* str) { return Append(str); }

  SimpleStringBuilder& operator<<(char ch) { return Append(&ch, 1); }

  SimpleStringBuilder& operator<<(const std::string& str) {
    return Append(str.c_str(), str.length());
  }

  // Numeric conversion routines.
  //
  // We use std::[v]snprintf instead of std::to_string because:
  // * std::to_string relies on the current locale for formatting purposes,
  //   and therefore concurrent calls to std::to_string from multiple threads
  //   may result in partial serialization of calls
  // * snprintf allows us to print the number directly into our buffer.
  // * avoid allocating a std::string (potential heap alloc).
  // TODO(tommi): Switch to std::to_chars in C++17.

  SimpleStringBuilder& operator<<(int i) { return AppendFormat("%d", i); }

  SimpleStringBuilder& operator<<(unsigned i) { return AppendFormat("%u", i); }

  SimpleStringBuilder& operator<<(long i) {  // NOLINT
    return AppendFormat("%ld", i);
  }

  SimpleStringBuilder& operator<<(long long i) {  // NOLINT
    return AppendFormat("%lld", i);
  }

  SimpleStringBuilder& operator<<(unsigned long i) {  // NOLINT
    return AppendFormat("%lu", i);
  }

  SimpleStringBuilder& operator<<(unsigned long long i) {  // NOLINT
    return AppendFormat("%llu", i);
  }

  SimpleStringBuilder& operator<<(float f) { return AppendFormat("%f", f); }

  SimpleStringBuilder& operator<<(double f) { return AppendFormat("%f", f); }

  SimpleStringBuilder& operator<<(long double f) {
    return AppendFormat("%Lf", f);
  }

  // Returns a pointer to the built string. The name |str()| is borrowed for
  // compatibility reasons as we replace usage of stringstream throughout the
  // code base.
  const char* str() const { return buffer_.data(); }

  // Returns the length of the string. The name |size()| is picked for STL
  // compatibility reasons.
  size_t size() const { return size_; }

  // Allows appending a printf style formatted string.
#if defined(__GNUC__)
  __attribute__((__format__(__printf__, 2, 3)))
#endif
  SimpleStringBuilder&
  AppendFormat(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const int len =
        std::vsnprintf(&buffer_[size_], buffer_.size() - size_, fmt, args);
    if (len >= 0) {
      const size_t chars_added = rtc::SafeMin(len, buffer_.size() - 1 - size_);
      size_ += chars_added;
      RTC_DCHECK_EQ(len, chars_added) << "Buffer size was insufficient";
    } else {
      // This should never happen, but we're paranoid, so re-write the
      // terminator in case vsnprintf() overwrote it.
      RTC_NOTREACHED();
      buffer_[size_] = '\0';
    }
    va_end(args);
    RTC_DCHECK(IsConsistent());
    return *this;
  }

  // An alternate way from operator<<() to append a string. This variant is
  // slightly more efficient when the length of the string to append, is known.
  SimpleStringBuilder& Append(const char* str, size_t length = SIZE_UNKNOWN) {
    const size_t chars_added =
        rtc::strcpyn(&buffer_[size_], buffer_.size() - size_, str, length);
    size_ += chars_added;
    RTC_DCHECK_EQ(chars_added,
                  length == SIZE_UNKNOWN ? std::strlen(str) : length)
        << "Buffer size was insufficient";
    RTC_DCHECK(IsConsistent());
    return *this;
  }

 private:
  bool IsConsistent() const {
    return size_ <= buffer_.size() - 1 && buffer_[size_] == '\0';
  }

  // An always-zero-terminated fixed-size buffer that we write to. The fixed
  // size allows the buffer to be stack allocated, which helps performance.
  // Having a fixed size is furthermore useful to avoid unnecessary resizing
  // while building it.
  const rtc::ArrayView<char> buffer_;

  // Represents the number of characters written to the buffer.
  // This does not include the terminating '\0'.
  size_t size_ = 0;
};

}  // namespace rtc

#endif  // RTC_BASE_STRINGS_STRING_BUILDER_H_
