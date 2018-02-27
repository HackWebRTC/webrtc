/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/string_builder.h"

#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stringutils.h"

namespace rtc {

TEST(SimpleStringBuilder, Limit) {
  SimpleStringBuilder<10> sb;
  EXPECT_EQ(0u, strlen(sb.str()));

  // Test that for a SSB with a buffer size of 10, that we can write 9 chars
  // into it.
  sb << "012345678";  // 9 characters + '\0'.
  EXPECT_EQ(0, strcmp(sb.str(), "012345678"));
}

TEST(SimpleStringBuilder, NumbersAndChars) {
  SimpleStringBuilder<100> sb;
  sb << 1 << ':' << 2.1 << ":" << 2.2f << ':' << 78187493520ll << ':'
     << 78187493520ul;
  EXPECT_EQ(0, strcmp(sb.str(), "1:2.100000:2.200000:78187493520:78187493520"));
}

TEST(SimpleStringBuilder, Format) {
  SimpleStringBuilder<100> sb;
  sb << "Here we go - ";
  sb.AppendFormat("This is a hex formatted value: 0x%08x", 3735928559);
  EXPECT_EQ(0,
            strcmp(sb.str(),
                   "Here we go - This is a hex formatted value: 0xdeadbeef"));
}

TEST(SimpleStringBuilder, StdString) {
  SimpleStringBuilder<100> sb;
  std::string str = "does this work?";
  sb << str;
  EXPECT_EQ(str, sb.str());
}

}  // namespace rtc
