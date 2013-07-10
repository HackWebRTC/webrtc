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

#include "talk/base/gunit.h"
#include "talk/base/winfirewall.h"

#include <objbase.h>

namespace talk_base {

TEST(WinFirewallTest, ReadStatus) {
  ::CoInitialize(NULL);
  WinFirewall fw;
  HRESULT hr;
  bool authorized;

  EXPECT_FALSE(fw.QueryAuthorized("bogus.exe", &authorized));
  EXPECT_TRUE(fw.Initialize(&hr));
  EXPECT_EQ(S_OK, hr);

  EXPECT_TRUE(fw.QueryAuthorized("bogus.exe", &authorized));

  // Unless we mock out INetFwMgr we can't really have an expectation either way
  // about whether we're authorized.  It will depend on the settings of the
  // machine running the test.  Same goes for AddApplication.

  fw.Shutdown();
  EXPECT_FALSE(fw.QueryAuthorized("bogus.exe", &authorized));

  ::CoUninitialize();
}

}  // namespace talk_base
