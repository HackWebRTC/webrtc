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

#ifndef _xmppstanzaparser_h_
#define _xmppstanzaparser_h_

#include "talk/xmllite/xmlparser.h"
#include "talk/xmllite/xmlbuilder.h"


namespace buzz {

class XmlElement;

class XmppStanzaParseHandler {
public:
  virtual ~XmppStanzaParseHandler() {}
  virtual void StartStream(const XmlElement * pelStream) = 0;
  virtual void Stanza(const XmlElement * pelStanza) = 0;
  virtual void EndStream() = 0;
  virtual void XmlError() = 0;
};

class XmppStanzaParser {
public:
  XmppStanzaParser(XmppStanzaParseHandler *psph);
  bool Parse(const char * data, size_t len, bool isFinal)
    { return parser_.Parse(data, len, isFinal); }
  void Reset();

private:
  class ParseHandler : public XmlParseHandler {
  public:
    ParseHandler(XmppStanzaParser * outer) : outer_(outer) {}
    virtual void StartElement(XmlParseContext * pctx,
               const char * name, const char ** atts)
      { outer_->IncomingStartElement(pctx, name, atts); }
    virtual void EndElement(XmlParseContext * pctx,
               const char * name)
      { outer_->IncomingEndElement(pctx, name); }
    virtual void CharacterData(XmlParseContext * pctx,
               const char * text, int len)
      { outer_->IncomingCharacterData(pctx, text, len); }
    virtual void Error(XmlParseContext * pctx,
               XML_Error errCode)
      { outer_->IncomingError(pctx, errCode); }
  private:
    XmppStanzaParser * const outer_;
  };

  friend class ParseHandler;

  void IncomingStartElement(XmlParseContext * pctx,
               const char * name, const char ** atts);
  void IncomingEndElement(XmlParseContext * pctx,
               const char * name);
  void IncomingCharacterData(XmlParseContext * pctx,
               const char * text, int len);
  void IncomingError(XmlParseContext * pctx,
               XML_Error errCode);

  XmppStanzaParseHandler * psph_;
  ParseHandler innerHandler_;
  XmlParser parser_;
  int depth_;
  XmlBuilder builder_;

 };


}

#endif
