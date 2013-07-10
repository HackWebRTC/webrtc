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

#ifndef TALK_XMPP_UTIL_UNITTEST_H_
#define TALK_XMPP_UTIL_UNITTEST_H_

#include <string>
#include <sstream>
#include "talk/xmpp/xmppengine.h"

namespace buzz {

// This class captures callbacks from engine.
class XmppTestHandler : public XmppOutputHandler,  public XmppSessionHandler,
                        public XmppStanzaHandler {
 public:
  explicit XmppTestHandler(XmppEngine* engine) : engine_(engine) {}
  virtual ~XmppTestHandler() {}

  void SetEngine(XmppEngine* engine);

  // Output handler
  virtual void WriteOutput(const char * bytes, size_t len);
  virtual void StartTls(const std::string & cname);
  virtual void CloseConnection();

  // Session handler
  virtual void OnStateChange(int state);

  // Stanza handler
  virtual bool HandleStanza(const XmlElement* stanza);

  std::string OutputActivity();
  std::string SessionActivity();
  std::string StanzaActivity();

 private:
  XmppEngine* engine_;
  std::stringstream output_;
  std::stringstream session_;
  std::stringstream stanza_;
};

}  // namespace buzz

inline std::ostream& operator<<(std::ostream& os, const buzz::Jid& jid) {
  os << jid.Str();
  return os;
}

#endif  // TALK_XMPP_UTIL_UNITTEST_H_
