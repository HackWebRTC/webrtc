/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_XMLLITE_XMLNSSTACK_H_
#define TALK_XMLLITE_XMLNSSTACK_H_

#include <string>
#include <vector>
#include "talk/base/scoped_ptr.h"
#include "talk/xmllite/qname.h"

namespace buzz {

class XmlnsStack {
public:
  XmlnsStack();
  ~XmlnsStack();

  void AddXmlns(const std::string& prefix, const std::string& ns);
  void RemoveXmlns();
  void PushFrame();
  void PopFrame();
  void Reset();

  std::pair<std::string, bool> NsForPrefix(const std::string& prefix);
  bool PrefixMatchesNs(const std::string & prefix, const std::string & ns);
  std::pair<std::string, bool> PrefixForNs(const std::string& ns, bool isAttr);
  std::pair<std::string, bool> AddNewPrefix(const std::string& ns, bool isAttr);
  std::string FormatQName(const QName & name, bool isAttr);

private:

  talk_base::scoped_ptr<std::vector<std::string> > pxmlnsStack_;
  talk_base::scoped_ptr<std::vector<size_t> > pxmlnsDepthStack_;
};
}

#endif  // TALK_XMLLITE_XMLNSSTACK_H_
