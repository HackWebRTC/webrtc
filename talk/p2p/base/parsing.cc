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

#include "talk/p2p/base/parsing.h"

#include <algorithm>
#include <stdlib.h>
#include "talk/base/stringutils.h"

namespace {
static const char kTrue[] = "true";
static const char kOne[] = "1";
}

namespace cricket {

bool BadParse(const std::string& text, ParseError* err) {
  if (err != NULL) {
    err->text = text;
  }
  return false;
}

bool BadWrite(const std::string& text, WriteError* err) {
  if (err != NULL) {
    err->text = text;
  }
  return false;
}

std::string GetXmlAttr(const buzz::XmlElement* elem,
                       const buzz::QName& name,
                       const std::string& def) {
  std::string val = elem->Attr(name);
  return val.empty() ? def : val;
}

std::string GetXmlAttr(const buzz::XmlElement* elem,
                       const buzz::QName& name,
                       const char* def) {
    return GetXmlAttr(elem, name, std::string(def));
}

bool GetXmlAttr(const buzz::XmlElement* elem,
                const buzz::QName& name, bool def) {
  std::string val = elem->Attr(name);
  std::transform(val.begin(), val.end(), val.begin(), tolower);

  return val.empty() ? def : (val == kTrue || val == kOne);
}

int GetXmlAttr(const buzz::XmlElement* elem,
               const buzz::QName& name, int def) {
  std::string val = elem->Attr(name);
  return val.empty() ? def : atoi(val.c_str());
}

const buzz::XmlElement* GetXmlChild(const buzz::XmlElement* parent,
                                    const std::string& name) {
  for (const buzz::XmlElement* child = parent->FirstElement();
       child != NULL;
       child = child->NextElement()) {
    if (child->Name().LocalPart() == name) {
      return child;
    }
  }
  return NULL;
}

bool RequireXmlChild(const buzz::XmlElement* parent,
                     const std::string& name,
                     const buzz::XmlElement** child,
                     ParseError* error) {
  *child = GetXmlChild(parent, name);
  if (*child == NULL) {
    return BadParse("element '" + parent->Name().Merged() +
                    "' missing required child '" + name,
                    error);
  } else {
    return true;
  }
}

bool RequireXmlAttr(const buzz::XmlElement* elem,
                    const buzz::QName& name,
                    std::string* value,
                    ParseError* error) {
  if (!elem->HasAttr(name)) {
    return BadParse("element '" + elem->Name().Merged() +
                    "' missing required attribute '"
                    + name.Merged() + "'",
                    error);
  } else {
    *value = elem->Attr(name);
    return true;
  }
}

void AddXmlAttrIfNonEmpty(buzz::XmlElement* elem,
                          const buzz::QName name,
                          const std::string& value) {
  if (!value.empty()) {
    elem->AddAttr(name, value);
  }
}

void AddXmlChildren(buzz::XmlElement* parent,
                    const std::vector<buzz::XmlElement*>& children) {
  for (std::vector<buzz::XmlElement*>::const_iterator iter = children.begin();
       iter != children.end();
       iter++) {
    parent->AddElement(*iter);
  }
}

void CopyXmlChildren(const buzz::XmlElement* source, buzz::XmlElement* dest) {
  for (const buzz::XmlElement* child = source->FirstElement();
       child != NULL;
       child = child->NextElement()) {
    dest->AddElement(new buzz::XmlElement(*child));
  }
}

std::vector<buzz::XmlElement*> CopyOfXmlChildren(const buzz::XmlElement* elem) {
  std::vector<buzz::XmlElement*> children;
  for (const buzz::XmlElement* child = elem->FirstElement();
       child != NULL;
       child = child->NextElement()) {
    children.push_back(new buzz::XmlElement(*child));
  }
  return children;
}

}  // namespace cricket
