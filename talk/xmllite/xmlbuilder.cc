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

#include "talk/xmllite/xmlbuilder.h"

#include <vector>
#include <set>
#include "talk/base/common.h"
#include "talk/xmllite/xmlconstants.h"
#include "talk/xmllite/xmlelement.h"

namespace buzz {

XmlBuilder::XmlBuilder() :
  pelCurrent_(NULL),
  pelRoot_(NULL),
  pvParents_(new std::vector<XmlElement *>()) {
}

void
XmlBuilder::Reset() {
  pelRoot_.reset();
  pelCurrent_ = NULL;
  pvParents_->clear();
}

XmlElement *
XmlBuilder::BuildElement(XmlParseContext * pctx,
                              const char * name, const char ** atts) {
  QName tagName(pctx->ResolveQName(name, false));
  if (tagName.IsEmpty())
    return NULL;

  XmlElement * pelNew = new XmlElement(tagName);

  if (!*atts)
    return pelNew;

  std::set<QName> seenNonlocalAtts;

  while (*atts) {
    QName attName(pctx->ResolveQName(*atts, true));
    if (attName.IsEmpty()) {
      delete pelNew;
      return NULL;
    }

    // verify that namespaced names are unique
    if (!attName.Namespace().empty()) {
      if (seenNonlocalAtts.count(attName)) {
        delete pelNew;
        return NULL;
      }
      seenNonlocalAtts.insert(attName);
    }

    pelNew->AddAttr(attName, std::string(*(atts + 1)));
    atts += 2;
  }

  return pelNew;
}

void
XmlBuilder::StartElement(XmlParseContext * pctx,
                              const char * name, const char ** atts) {
  XmlElement * pelNew = BuildElement(pctx, name, atts);
  if (pelNew == NULL) {
    pctx->RaiseError(XML_ERROR_SYNTAX);
    return;
  }

  if (!pelCurrent_) {
    pelCurrent_ = pelNew;
    pelRoot_.reset(pelNew);
    pvParents_->push_back(NULL);
  } else {
    pelCurrent_->AddElement(pelNew);
    pvParents_->push_back(pelCurrent_);
    pelCurrent_ = pelNew;
  }
}

void
XmlBuilder::EndElement(XmlParseContext * pctx, const char * name) {
  UNUSED(pctx);
  UNUSED(name);
  pelCurrent_ = pvParents_->back();
  pvParents_->pop_back();
}

void
XmlBuilder::CharacterData(XmlParseContext * pctx,
                               const char * text, int len) {
  UNUSED(pctx);
  if (pelCurrent_) {
    pelCurrent_->AddParsedText(text, len);
  }
}

void
XmlBuilder::Error(XmlParseContext * pctx, XML_Error err) {
  UNUSED(pctx);
  UNUSED(err);
  pelRoot_.reset(NULL);
  pelCurrent_ = NULL;
  pvParents_->clear();
}

XmlElement *
XmlBuilder::CreateElement() {
  return pelRoot_.release();
}

XmlElement *
XmlBuilder::BuiltElement() {
  return pelRoot_.get();
}

XmlBuilder::~XmlBuilder() {
}

}  // namespace buzz
