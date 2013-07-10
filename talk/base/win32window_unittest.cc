/*
 * libjingle
 * Copyright 2009, Google Inc.
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
#include "talk/base/common.h"
#include "talk/base/win32window.h"
#include "talk/base/logging.h"

static LRESULT kDummyResult = 0x1234ABCD;

class TestWindow : public talk_base::Win32Window {
 public:
  TestWindow() : destroyed_(false) { memset(&msg_, 0, sizeof(msg_)); }
  const MSG& msg() const { return msg_; }
  bool destroyed() const { return destroyed_; }

  virtual bool OnMessage(UINT uMsg, WPARAM wParam,
                         LPARAM lParam, LRESULT& result) {
    msg_.message = uMsg;
    msg_.wParam = wParam;
    msg_.lParam = lParam;
    result = kDummyResult;
    return true;
  }
  virtual void OnNcDestroy() {
    destroyed_ = true;
  }

 private:
  MSG msg_;
  bool destroyed_;
};

TEST(Win32WindowTest, Basics) {
  TestWindow wnd;
  EXPECT_TRUE(wnd.handle() == NULL);
  EXPECT_FALSE(wnd.destroyed());
  EXPECT_TRUE(wnd.Create(0, L"Test", 0, 0, 0, 0, 100, 100));
  EXPECT_TRUE(wnd.handle() != NULL);
  EXPECT_EQ(kDummyResult, ::SendMessage(wnd.handle(), WM_USER, 1, 2));
  EXPECT_EQ(WM_USER, wnd.msg().message);
  EXPECT_EQ(1, wnd.msg().wParam);
  EXPECT_EQ(2, wnd.msg().lParam);
  wnd.Destroy();
  EXPECT_TRUE(wnd.handle() == NULL);
  EXPECT_TRUE(wnd.destroyed());
}

TEST(Win32WindowTest, MultipleWindows) {
  TestWindow wnd1, wnd2;
  EXPECT_TRUE(wnd1.Create(0, L"Test", 0, 0, 0, 0, 100, 100));
  EXPECT_TRUE(wnd2.Create(0, L"Test", 0, 0, 0, 0, 100, 100));
  EXPECT_TRUE(wnd1.handle() != NULL);
  EXPECT_TRUE(wnd2.handle() != NULL);
  wnd1.Destroy();
  wnd2.Destroy();
  EXPECT_TRUE(wnd2.handle() == NULL);
  EXPECT_TRUE(wnd1.handle() == NULL);
}
