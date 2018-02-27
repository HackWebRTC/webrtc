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
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"

namespace rtc {

// This is a minimalistic string builder class meant to cover the most cases
// of when you might otherwise be tempted to use a stringstream (discouraged
// for anything except logging).
// This class allocates a fixed size buffer on the stack and concatenates
// strings and numbers into it, allowing the results to be read via |str()|.
template <size_t buffer_size>
class SimpleStringBuilder {
 public:
  SimpleStringBuilder() { buffer_[0] = '\0'; }
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
  const char* str() const { return &buffer_[0]; }

  // Returns the length of the string. The name |size()| is picked for STL
  // compatibility reasons.
  size_t size() const { return size_; }

  // Allows appending a printf style formatted string.
  SimpleStringBuilder& AppendFormat(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = std::vsnprintf(&buffer_[size_], buffer_size - size_, fmt, args);
    RTC_DCHECK_GE(len, 0);
    // Negative values are likely programmer error, but let's not update the
    // length if so.
    if (len > 0)
      AddToLength(len);
    va_end(args);
    return *this;
  }

  // An alternate way from operator<<() to append a string. This variant is
  // slightly more efficient when the length of the string to append, is known.
  SimpleStringBuilder& Append(const char* str, size_t length = SIZE_UNKNOWN) {
    AddToLength(
        rtc::strcpyn(&buffer_[size_], buffer_size - size_, str, length));
    return *this;
  }

 private:
  void AddToLength(size_t chars_added) {
    size_ += chars_added;
    RTC_DCHECK_EQ('\0', buffer_[size_]);
    RTC_DCHECK_LE(size_, buffer_size - 1)
        << "Buffer size limit reached (" << buffer_size << ")";
  }

  // An always-zero-terminated fixed buffer that we write to.
  // Assuming the SimpleStringBuilder instance lives on the stack, this
  // buffer will be stack allocated, which is done for performance reasons.
  // Having a fixed size is furthermore useful to avoid unnecessary resizing
  // while building it.
  char buffer_[buffer_size];  // NOLINT

  // Represents the number of characters written to the buffer.
  // This does not include the terminating '\0'.
  size_t size_ = 0;
};

}  // namespace rtc

#endif  // RTC_BASE_STRINGS_STRING_BUILDER_H_
