/*
 * libjingle
 * Copyright 2010, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>

#include "talk/base/gunit.h"
#include "talk/base/nethelpers.h"
#include "talk/base/win32.h"
#include "talk/base/winping.h"

#ifndef WIN32
#error Only for Windows
#endif

namespace talk_base {

class Win32Test : public testing::Test {
 public:
  Win32Test() {
  }
};

TEST_F(Win32Test, FileTimeToUInt64Test) {
  FILETIME ft;
  ft.dwHighDateTime = 0xBAADF00D;
  ft.dwLowDateTime = 0xFEED3456;

  uint64 expected = 0xBAADF00DFEED3456;
  EXPECT_EQ(expected, ToUInt64(ft));
}

TEST_F(Win32Test, WinPingTest) {
  WinPing ping;
  ASSERT_TRUE(ping.IsValid());

  // Test valid ping cases.
  WinPing::PingResult result = ping.Ping(IPAddress(INADDR_LOOPBACK), 20, 50, 1,
                                         false);
  ASSERT_EQ(WinPing::PING_SUCCESS, result);
  if (HasIPv6Enabled()) {
    WinPing::PingResult v6result = ping.Ping(IPAddress(in6addr_loopback), 20,
                                             50, 1, false);
    ASSERT_EQ(WinPing::PING_SUCCESS, v6result);
  }

  // Test invalid parameter cases.
  ASSERT_EQ(WinPing::PING_INVALID_PARAMS, ping.Ping(
            IPAddress(INADDR_LOOPBACK), 0, 50, 1, false));
  ASSERT_EQ(WinPing::PING_INVALID_PARAMS, ping.Ping(
            IPAddress(INADDR_LOOPBACK), 20, 0, 1, false));
  ASSERT_EQ(WinPing::PING_INVALID_PARAMS, ping.Ping(
            IPAddress(INADDR_LOOPBACK), 20, 50, 0, false));
}

}  // namespace talk_base
