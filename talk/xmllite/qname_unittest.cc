/*
 * libjingle
 * Copyright 2004, Google Inc.
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
#include "talk/xmllite/qname.h"

using buzz::StaticQName;
using buzz::QName;

TEST(QNameTest, TestTrivial) {
  QName name("test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "");
}

TEST(QNameTest, TestSplit) {
  QName name("a:test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "a");
  QName name2("a-very:long:namespace:test-this");
  EXPECT_EQ(name2.LocalPart(), "test-this");
  EXPECT_EQ(name2.Namespace(), "a-very:long:namespace");
}

TEST(QNameTest, TestMerge) {
  QName name("a", "test");
  EXPECT_EQ(name.LocalPart(), "test");
  EXPECT_EQ(name.Namespace(), "a");
  EXPECT_EQ(name.Merged(), "a:test");
  QName name2("a-very:long:namespace", "test-this");
  EXPECT_EQ(name2.LocalPart(), "test-this");
  EXPECT_EQ(name2.Namespace(), "a-very:long:namespace");
  EXPECT_EQ(name2.Merged(), "a-very:long:namespace:test-this");
}

TEST(QNameTest, TestAssignment) {
  QName name("a", "test");
  // copy constructor
  QName namecopy(name);
  EXPECT_EQ(namecopy.LocalPart(), "test");
  EXPECT_EQ(namecopy.Namespace(), "a");
  QName nameassigned("");
  nameassigned = name;
  EXPECT_EQ(nameassigned.LocalPart(), "test");
  EXPECT_EQ(nameassigned.Namespace(), "a");
}

TEST(QNameTest, TestConstAssignment) {
  StaticQName name = { "a", "test" };
  QName namecopy(name);
  EXPECT_EQ(namecopy.LocalPart(), "test");
  EXPECT_EQ(namecopy.Namespace(), "a");
  QName nameassigned("");
  nameassigned = name;
  EXPECT_EQ(nameassigned.LocalPart(), "test");
  EXPECT_EQ(nameassigned.Namespace(), "a");
}

TEST(QNameTest, TestEquality) {
  QName name("a-very:long:namespace:test-this");
  QName name2("a-very:long:namespace", "test-this");
  QName name3("a-very:long:namespaxe", "test-this");
  EXPECT_TRUE(name == name2);
  EXPECT_FALSE(name == name3);
}

TEST(QNameTest, TestCompare) {
  QName name("a");
  QName name2("nsa", "a");
  QName name3("nsa", "b");
  QName name4("nsb", "b");

  EXPECT_TRUE(name < name2);
  EXPECT_FALSE(name2 < name);

  EXPECT_FALSE(name2 < name2);

  EXPECT_TRUE(name2 < name3);
  EXPECT_FALSE(name3 < name2);

  EXPECT_TRUE(name3 < name4);
  EXPECT_FALSE(name4 < name3);
}

TEST(QNameTest, TestStaticQName) {
  const StaticQName const_name1 = { "namespace", "local-name1" };
  const StaticQName const_name2 = { "namespace", "local-name2" };
  const QName name("namespace", "local-name1");
  const QName name1 = const_name1;
  const QName name2 = const_name2;

  EXPECT_TRUE(name == const_name1);
  EXPECT_TRUE(const_name1 == name);
  EXPECT_FALSE(name != const_name1);
  EXPECT_FALSE(const_name1 != name);

  EXPECT_TRUE(name == name1);
  EXPECT_TRUE(name1 == name);
  EXPECT_FALSE(name != name1);
  EXPECT_FALSE(name1 != name);

  EXPECT_FALSE(name == name2);
  EXPECT_FALSE(name2 == name);
  EXPECT_TRUE(name != name2);
  EXPECT_TRUE(name2 != name);
}
