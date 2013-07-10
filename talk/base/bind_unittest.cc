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
#include "talk/base/gunit.h"

namespace talk_base {

namespace {

struct MethodBindTester {
  void NullaryVoid() { ++call_count; }
  int NullaryInt() { ++call_count; return 1; }
  int NullaryConst() const { ++call_count; return 2; }
  void UnaryVoid(int dummy) { ++call_count; }
  template <class T> T Identity(T value) { ++call_count; return value; }
  int UnaryByRef(int& value) const { ++call_count; return ++value; }  // NOLINT
  int Multiply(int a, int b) const { ++call_count; return a * b; }
  mutable int call_count;
};

}  // namespace

TEST(BindTest, BindToMethod) {
  MethodBindTester object = {0};
  EXPECT_EQ(0, object.call_count);
  Bind(&MethodBindTester::NullaryVoid, &object)();
  EXPECT_EQ(1, object.call_count);
  EXPECT_EQ(1, Bind(&MethodBindTester::NullaryInt, &object)());
  EXPECT_EQ(2, object.call_count);
  EXPECT_EQ(2, Bind(&MethodBindTester::NullaryConst,
                    static_cast<const MethodBindTester*>(&object))());
  EXPECT_EQ(3, object.call_count);
  Bind(&MethodBindTester::UnaryVoid, &object, 5)();
  EXPECT_EQ(4, object.call_count);
  EXPECT_EQ(100, Bind(&MethodBindTester::Identity<int>, &object, 100)());
  EXPECT_EQ(5, object.call_count);
  const std::string string_value("test string");
  EXPECT_EQ(string_value, Bind(&MethodBindTester::Identity<std::string>,
                               &object, string_value)());
  EXPECT_EQ(6, object.call_count);
  int value = 11;
  EXPECT_EQ(12, Bind(&MethodBindTester::UnaryByRef, &object, value)());
  EXPECT_EQ(12, value);
  EXPECT_EQ(7, object.call_count);
  EXPECT_EQ(56, Bind(&MethodBindTester::Multiply, &object, 7, 8)());
  EXPECT_EQ(8, object.call_count);
}

}  // namespace talk_base
