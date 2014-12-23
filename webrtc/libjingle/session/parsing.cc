/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/session/parsing.h"

#include <stdlib.h>
#include <algorithm>
#include "webrtc/base/stringutils.h"

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
