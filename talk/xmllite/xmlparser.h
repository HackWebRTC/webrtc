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

#ifndef TALK_XMLLITE_XMLPARSER_H_
#define TALK_XMLLITE_XMLPARSER_H_

#include <string>

#include "talk/xmllite/xmlnsstack.h"
#ifdef EXPAT_RELATIVE_PATH
#include "expat.h"
#else
#include "third_party/expat/v2_0_1/Source/lib/expat.h"
#endif  // EXPAT_RELATIVE_PATH

struct XML_ParserStruct;
typedef struct XML_ParserStruct* XML_Parser;

namespace buzz {

class XmlParseHandler;
class XmlParseContext;
class XmlParser;

class XmlParseContext {
public:
  virtual ~XmlParseContext() {}
  virtual QName ResolveQName(const char * qname, bool isAttr) = 0;
  virtual void RaiseError(XML_Error err) = 0;
  virtual void GetPosition(unsigned long * line, unsigned long * column,
                           unsigned long * byte_index) = 0;
};

class XmlParseHandler {
public:
  virtual ~XmlParseHandler() {}
  virtual void StartElement(XmlParseContext * pctx,
               const char * name, const char ** atts) = 0;
  virtual void EndElement(XmlParseContext * pctx,
               const char * name) = 0;
  virtual void CharacterData(XmlParseContext * pctx,
               const char * text, int len) = 0;
  virtual void Error(XmlParseContext * pctx,
               XML_Error errorCode) = 0;
};

class XmlParser {
public:
  static void ParseXml(XmlParseHandler * pxph, std::string text);

  explicit XmlParser(XmlParseHandler * pxph);
  bool Parse(const char * data, size_t len, bool isFinal);
  void Reset();
  virtual ~XmlParser();

  // expat callbacks
  void ExpatStartElement(const char * name, const char ** atts);
  void ExpatEndElement(const char * name);
  void ExpatCharacterData(const char * text, int len);
  void ExpatXmlDecl(const char * ver, const char * enc, int standalone);

private:

  class ParseContext : public XmlParseContext {
  public:
    ParseContext(XmlParser * parser);
    virtual ~ParseContext();
    virtual QName ResolveQName(const char * qname, bool isAttr);
    virtual void RaiseError(XML_Error err) { if (!raised_) raised_ = err; }
    virtual void GetPosition(unsigned long * line, unsigned long * column,
                             unsigned long * byte_index);
    XML_Error RaisedError() { return raised_; }
    void Reset();

    void StartElement();
    void EndElement();
    void StartNamespace(const char * prefix, const char * ns);
    void SetPosition(int line, int column, long byte_index);

  private:
    const XmlParser * parser_;
    XmlnsStack xmlnsstack_;
    XML_Error raised_;
    XML_Size line_number_;
    XML_Size column_number_;
    XML_Index byte_index_;
  };

  ParseContext context_;
  XML_Parser expat_;
  XmlParseHandler * pxph_;
  bool sentError_;
};

}  // namespace buzz

#endif  // TALK_XMLLITE_XMLPARSER_H_
