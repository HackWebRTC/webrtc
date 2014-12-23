/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_PARSING_H_
#define WEBRTC_LIBJINGLE_SESSION_PARSING_H_

#include <string>
#include <vector>
#include "webrtc/libjingle/xmllite/xmlelement.h"  // Needed to delete ParseError.extra.
#include "webrtc/base/basictypes.h"
#include "webrtc/base/stringencode.h"

namespace cricket {

typedef std::vector<buzz::XmlElement*> XmlElements;

// We decided "bool Parse(in, out*, error*)" is generally the best
// parse signature.  "out Parse(in)" doesn't allow for errors.
// "error* Parse(in, out*)" doesn't allow flexible memory management.

// The error type for parsing.
struct ParseError {
 public:
  // explains the error
  std::string text;
  // provide details about what wasn't parsable
  const buzz::XmlElement* extra;

  ParseError() : extra(NULL) {}

  ~ParseError() {
    delete extra;
  }

  void SetText(const std::string& text) {
    this->text = text;
  }
};

// The error type for writing.
struct WriteError {
  std::string text;

  void SetText(const std::string& text) {
    this->text = text;
  }
};

// Convenience method for returning a message when parsing fails.
bool BadParse(const std::string& text, ParseError* err);

// Convenience method for returning a message when writing fails.
bool BadWrite(const std::string& text, WriteError* error);

// helper XML functions
std::string GetXmlAttr(const buzz::XmlElement* elem,
                       const buzz::QName& name,
                       const std::string& def);
std::string GetXmlAttr(const buzz::XmlElement* elem,
                       const buzz::QName& name,
                       const char* def);
// Return true if the value is "true" or "1".
bool GetXmlAttr(const buzz::XmlElement* elem,
                const buzz::QName& name, bool def);
int GetXmlAttr(const buzz::XmlElement* elem,
               const buzz::QName& name, int def);

template <class T>
bool GetXmlAttr(const buzz::XmlElement* elem,
                const buzz::QName& name,
                T* val_out) {
  if (!elem->HasAttr(name)) {
    return false;
  }
  std::string unparsed = elem->Attr(name);
  return rtc::FromString(unparsed, val_out);
}

template <class T>
bool GetXmlAttr(const buzz::XmlElement* elem,
                const buzz::QName& name,
                const T& def,
                T* val_out) {
  if (!elem->HasAttr(name)) {
    *val_out = def;
    return true;
  }
  return GetXmlAttr(elem, name, val_out);
}

template <class T>
bool AddXmlAttr(buzz::XmlElement* elem,
                const buzz::QName& name, const T& val) {
  std::string buf;
  if (!rtc::ToString(val, &buf)) {
    return false;
  }
  elem->AddAttr(name, buf);
  return true;
}

template <class T>
bool SetXmlBody(buzz::XmlElement* elem, const T& val) {
  std::string buf;
  if (!rtc::ToString(val, &buf)) {
    return false;
  }
  elem->SetBodyText(buf);
  return true;
}

const buzz::XmlElement* GetXmlChild(const buzz::XmlElement* parent,
                                    const std::string& name);

bool RequireXmlChild(const buzz::XmlElement* parent,
                     const std::string& name,
                     const buzz::XmlElement** child,
                     ParseError* error);
bool RequireXmlAttr(const buzz::XmlElement* elem,
                    const buzz::QName& name,
                    std::string* value,
                    ParseError* error);
void AddXmlAttrIfNonEmpty(buzz::XmlElement* elem,
                          const buzz::QName name,
                          const std::string& value);
void AddXmlChildren(buzz::XmlElement* parent,
                    const std::vector<buzz::XmlElement*>& children);
void CopyXmlChildren(const buzz::XmlElement* source, buzz::XmlElement* dest);
std::vector<buzz::XmlElement*> CopyOfXmlChildren(const buzz::XmlElement* elem);

}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_PARSING_H_
