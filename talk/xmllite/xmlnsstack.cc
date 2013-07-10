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

#include "talk/xmllite/xmlnsstack.h"

#include <sstream>
#include <string>
#include <vector>

#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/xmlconstants.h"

namespace buzz {

XmlnsStack::XmlnsStack() :
  pxmlnsStack_(new std::vector<std::string>),
  pxmlnsDepthStack_(new std::vector<size_t>) {
}

XmlnsStack::~XmlnsStack() {}

void XmlnsStack::PushFrame() {
  pxmlnsDepthStack_->push_back(pxmlnsStack_->size());
}

void XmlnsStack::PopFrame() {
  size_t prev_size = pxmlnsDepthStack_->back();
  pxmlnsDepthStack_->pop_back();
  if (prev_size < pxmlnsStack_->size()) {
    pxmlnsStack_->erase(pxmlnsStack_->begin() + prev_size,
                        pxmlnsStack_->end());
  }
}

std::pair<std::string, bool> XmlnsStack::NsForPrefix(
    const std::string& prefix) {
  if (prefix.length() >= 3 &&
      (prefix[0] == 'x' || prefix[0] == 'X') &&
      (prefix[1] == 'm' || prefix[1] == 'M') &&
      (prefix[2] == 'l' || prefix[2] == 'L')) {
    if (prefix == "xml")
      return std::make_pair(NS_XML, true);
    if (prefix == "xmlns")
      return std::make_pair(NS_XMLNS, true);
    // Other names with xml prefix are illegal.
    return std::make_pair(STR_EMPTY, false);
  }

  std::vector<std::string>::iterator pos;
  for (pos = pxmlnsStack_->end(); pos > pxmlnsStack_->begin(); ) {
    pos -= 2;
    if (*pos == prefix)
      return std::make_pair(*(pos + 1), true);
  }

  if (prefix == STR_EMPTY)
    return std::make_pair(STR_EMPTY, true);  // default namespace

  return std::make_pair(STR_EMPTY, false);  // none found
}

bool XmlnsStack::PrefixMatchesNs(const std::string& prefix,
                                 const std::string& ns) {
  const std::pair<std::string, bool> match = NsForPrefix(prefix);
  return match.second && (match.first == ns);
}

std::pair<std::string, bool> XmlnsStack::PrefixForNs(const std::string& ns,
                                                     bool isattr) {
  if (ns == NS_XML)
    return std::make_pair(std::string("xml"), true);
  if (ns == NS_XMLNS)
    return std::make_pair(std::string("xmlns"), true);
  if (isattr ? ns == STR_EMPTY : PrefixMatchesNs(STR_EMPTY, ns))
    return std::make_pair(STR_EMPTY, true);

  std::vector<std::string>::iterator pos;
  for (pos = pxmlnsStack_->end(); pos > pxmlnsStack_->begin(); ) {
    pos -= 2;
    if (*(pos + 1) == ns &&
        (!isattr || !pos->empty()) && PrefixMatchesNs(*pos, ns))
      return std::make_pair(*pos, true);
  }

  return std::make_pair(STR_EMPTY, false); // none found
}

std::string XmlnsStack::FormatQName(const QName& name, bool isAttr) {
  std::string prefix(PrefixForNs(name.Namespace(), isAttr).first);
  if (prefix == STR_EMPTY)
    return name.LocalPart();
  else
    return prefix + ':' + name.LocalPart();
}

void XmlnsStack::AddXmlns(const std::string & prefix, const std::string & ns) {
  pxmlnsStack_->push_back(prefix);
  pxmlnsStack_->push_back(ns);
}

void XmlnsStack::RemoveXmlns() {
  pxmlnsStack_->pop_back();
  pxmlnsStack_->pop_back();
}

static bool IsAsciiLetter(char ch) {
  return ((ch >= 'a' && ch <= 'z') ||
          (ch >= 'A' && ch <= 'Z'));
}

static std::string AsciiLower(const std::string & s) {
  std::string result(s);
  size_t i;
  for (i = 0; i < result.length(); i++) {
    if (result[i] >= 'A' && result[i] <= 'Z')
      result[i] += 'a' - 'A';
  }
  return result;
}

static std::string SuggestPrefix(const std::string & ns) {
  size_t len = ns.length();
  size_t i = ns.find_last_of('.');
  if (i != std::string::npos && len - i <= 4 + 1)
    len = i; // chop off ".html" or ".xsd" or ".?{0,4}"
  size_t last = len;
  while (last > 0) {
    last -= 1;
    if (IsAsciiLetter(ns[last])) {
      size_t first = last;
      last += 1;
      while (first > 0) {
        if (!IsAsciiLetter(ns[first - 1]))
          break;
        first -= 1;
      }
      if (last - first > 4)
        last = first + 3;
      std::string candidate(AsciiLower(ns.substr(first, last - first)));
      if (candidate.find("xml") != 0)
        return candidate;
      break;
    }
  }
  return "ns";
}

std::pair<std::string, bool> XmlnsStack::AddNewPrefix(const std::string& ns,
                                                      bool isAttr) {
  if (PrefixForNs(ns, isAttr).second)
    return std::make_pair(STR_EMPTY, false);

  std::string base(SuggestPrefix(ns));
  std::string result(base);
  int i = 2;
  while (NsForPrefix(result).second) {
    std::stringstream ss;
    ss << base;
    ss << (i++);
    ss >> result;
  }
  AddXmlns(result, ns);
  return std::make_pair(result, true);
}

void XmlnsStack::Reset() {
  pxmlnsStack_->clear();
  pxmlnsDepthStack_->clear();
}

}
