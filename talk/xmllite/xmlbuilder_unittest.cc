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
#include <sstream>
#include <iostream>
#include "talk/base/common.h"
#include "talk/base/gunit.h"
#include "talk/xmllite/xmlbuilder.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlparser.h"

using buzz::XmlBuilder;
using buzz::XmlElement;
using buzz::XmlParser;

TEST(XmlBuilderTest, TestTrivial) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing/>");
  EXPECT_EQ("<testing/>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestAttributes1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='b'/>");
  EXPECT_EQ("<testing a=\"b\"/>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestAttributes2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing e='' long='some text'/>");
  EXPECT_EQ("<testing e=\"\" long=\"some text\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestNesting1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
      "<top><first/><second><third></third></second></top>");
  EXPECT_EQ("<top><first/><second><third/></second></top>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestNesting2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
    "<top><fifth><deeper><and><deeper/></and><sibling><leaf/>"
    "</sibling></deeper></fifth><first/><second><third></third>"
    "</second></top>");
  EXPECT_EQ("<top><fifth><deeper><and><deeper/></and><sibling><leaf/>"
    "</sibling></deeper></fifth><first/><second><third/>"
    "</second></top>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestQuoting1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='>'/>");
  EXPECT_EQ("<testing a=\"&gt;\"/>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestQuoting2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='&lt;>&amp;&quot;'/>");
  EXPECT_EQ("<testing a=\"&lt;&gt;&amp;&quot;\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestQuoting3) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='so &quot;important&quot;'/>");
  EXPECT_EQ("<testing a=\"so &quot;important&quot;\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestQuoting4) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='&quot;important&quot;, yes'/>");
  EXPECT_EQ("<testing a=\"&quot;important&quot;, yes\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestQuoting5) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
      "<testing a='&lt;what is &quot;important&quot;&gt;'/>");
  EXPECT_EQ("<testing a=\"&lt;what is &quot;important&quot;&gt;\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestText1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing>></testing>");
  EXPECT_EQ("<testing>&gt;</testing>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestText2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing>&lt;>&amp;&quot;</testing>");
  EXPECT_EQ("<testing>&lt;&gt;&amp;\"</testing>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestText3) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing>so &lt;important&gt;</testing>");
  EXPECT_EQ("<testing>so &lt;important&gt;</testing>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestText4) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing>&lt;important&gt;, yes</testing>");
  EXPECT_EQ("<testing>&lt;important&gt;, yes</testing>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestText5) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
      "<testing>importance &amp;&lt;important&gt;&amp;</testing>");
  EXPECT_EQ("<testing>importance &amp;&lt;important&gt;&amp;</testing>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestNamespace1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing xmlns='foo'/>");
  EXPECT_EQ("<testing xmlns=\"foo\"/>", builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestNamespace2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing xmlns:a='foo' a:b='c'/>");
  EXPECT_EQ("<testing xmlns:a=\"foo\" a:b=\"c\"/>",
      builder.BuiltElement()->Str());
}

TEST(XmlBuilderTest, TestNamespace3) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing xmlns:a=''/>");
  EXPECT_TRUE(NULL == builder.BuiltElement());
}

TEST(XmlBuilderTest, TestNamespace4) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a:b='c'/>");
  EXPECT_TRUE(NULL == builder.BuiltElement());
}

TEST(XmlBuilderTest, TestAttrCollision1) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, "<testing a='first' a='second'/>");
  EXPECT_TRUE(NULL == builder.BuiltElement());
}

TEST(XmlBuilderTest, TestAttrCollision2) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
      "<testing xmlns:a='foo' xmlns:b='foo' a:x='c' b:x='d'/>");
  EXPECT_TRUE(NULL == builder.BuiltElement());
}

TEST(XmlBuilderTest, TestAttrCollision3) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder,
      "<testing xmlns:a='foo'><nested xmlns:b='foo' a:x='c' b:x='d'/>"
      "</testing>");
  EXPECT_TRUE(NULL == builder.BuiltElement());
}

