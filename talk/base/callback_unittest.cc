/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/base/bind.h"
#include "talk/base/callback.h"
#include "talk/base/gunit.h"

namespace talk_base {

namespace {

void f() {}
int g() { return 42; }
int h(int x) { return x * x; }
void i(int& x) { x *= x; }  // NOLINT: Testing refs

struct BindTester {
  int a() { return 24; }
  int b(int x) const { return x * x; }
};

}  // namespace

TEST(CallbackTest, VoidReturn) {
  Callback0<void> cb;
  EXPECT_TRUE(cb.empty());
  cb();  // Executing an empty callback should not crash.
  cb = Callback0<void>(&f);
  EXPECT_FALSE(cb.empty());
  cb();
}

TEST(CallbackTest, IntReturn) {
  Callback0<int> cb;
  EXPECT_TRUE(cb.empty());
  cb = Callback0<int>(&g);
  EXPECT_FALSE(cb.empty());
  EXPECT_EQ(42, cb());
  EXPECT_EQ(42, cb());
}

TEST(CallbackTest, OneParam) {
  Callback1<int, int> cb1(&h);
  EXPECT_FALSE(cb1.empty());
  EXPECT_EQ(9, cb1(-3));
  EXPECT_EQ(100, cb1(10));

  // Try clearing a callback.
  cb1 = Callback1<int, int>();
  EXPECT_TRUE(cb1.empty());

  // Try a callback with a ref parameter.
  Callback1<void, int&> cb2(&i);
  int x = 3;
  cb2(x);
  EXPECT_EQ(9, x);
  cb2(x);
  EXPECT_EQ(81, x);
}

TEST(CallbackTest, WithBind) {
  BindTester t;
  Callback0<int> cb1 = Bind(&BindTester::a, &t);
  EXPECT_EQ(24, cb1());
  EXPECT_EQ(24, cb1());
  cb1 = Bind(&BindTester::b, &t, 10);
  EXPECT_EQ(100, cb1());
  EXPECT_EQ(100, cb1());
  cb1 = Bind(&BindTester::b, &t, 5);
  EXPECT_EQ(25, cb1());
  EXPECT_EQ(25, cb1());
}

}  // namespace talk_base
