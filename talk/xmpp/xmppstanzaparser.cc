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

#include "talk/xmpp/xmppstanzaparser.h"

#include "talk/xmllite/xmlelement.h"
#include "talk/base/common.h"
#include "talk/xmpp/constants.h"
#ifdef EXPAT_RELATIVE_PATH
#include "expat.h"
#else
#include "third_party/expat/v2_0_1/Source/lib/expat.h"
#endif

namespace buzz {

XmppStanzaParser::XmppStanzaParser(XmppStanzaParseHandler *psph) :
  psph_(psph),
  innerHandler_(this),
  parser_(&innerHandler_),
  depth_(0),
  builder_() {
}

void
XmppStanzaParser::Reset() {
  parser_.Reset();
  depth_ = 0;
  builder_.Reset();
}

void
XmppStanzaParser::IncomingStartElement(
    XmlParseContext * pctx, const char * name, const char ** atts) {
  if (depth_++ == 0) {
    XmlElement * pelStream = XmlBuilder::BuildElement(pctx, name, atts);
    if (pelStream == NULL) {
      pctx->RaiseError(XML_ERROR_SYNTAX);
      return;
    }
    psph_->StartStream(pelStream);
    delete pelStream;
    return;
  }

  builder_.StartElement(pctx, name, atts);
}

void
XmppStanzaParser::IncomingCharacterData(
    XmlParseContext * pctx, const char * text, int len) {
  if (depth_ > 1) {
    builder_.CharacterData(pctx, text, len);
  }
}

void
XmppStanzaParser::IncomingEndElement(
    XmlParseContext * pctx, const char * name) {
  if (--depth_ == 0) {
    psph_->EndStream();
    return;
  }

  builder_.EndElement(pctx, name);

  if (depth_ == 1) {
    XmlElement *element = builder_.CreateElement();
    psph_->Stanza(element);
    delete element;
  }
}

void
XmppStanzaParser::IncomingError(
    XmlParseContext * pctx, XML_Error errCode) {
  UNUSED(pctx);
  UNUSED(errCode);
  psph_->XmlError();
}

}
