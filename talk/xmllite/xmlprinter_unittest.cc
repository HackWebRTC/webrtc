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

#include "talk/xmllite/xmlprinter.h"

#include <sstream>
#include <string>

#include "talk/base/common.h"
#include "talk/base/gunit.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlnsstack.h"

using buzz::QName;
using buzz::XmlElement;
using buzz::XmlnsStack;
using buzz::XmlPrinter;

TEST(XmlPrinterTest, TestBasicPrinting) {
  XmlElement elt(QName("google:test", "first"));
  std::stringstream ss;
  XmlPrinter::PrintXml(&ss, &elt);
  EXPECT_EQ("<test:first xmlns:test=\"google:test\"/>", ss.str());
}

TEST(XmlPrinterTest, TestNamespacedPrinting) {
  XmlElement elt(QName("google:test", "first"));
  elt.AddElement(new XmlElement(QName("nested:test", "second")));
  std::stringstream ss;

  XmlnsStack ns_stack;
  ns_stack.AddXmlns("gg", "google:test");
  ns_stack.AddXmlns("", "nested:test");

  XmlPrinter::PrintXml(&ss, &elt, &ns_stack);
  EXPECT_EQ("<gg:first><second/></gg:first>", ss.str());
}
