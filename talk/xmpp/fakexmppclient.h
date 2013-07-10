/*
 * libjingle
 * Copyright 2011, Google Inc.
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

// A fake XmppClient for use in unit tests.

#ifndef TALK_XMPP_FAKEXMPPCLIENT_H_
#define TALK_XMPP_FAKEXMPPCLIENT_H_

#include <string>
#include <vector>

#include "talk/xmpp/xmpptask.h"

namespace buzz {

class XmlElement;

class FakeXmppClient : public XmppTaskParentInterface,
                       public XmppClientInterface {
 public:
  explicit FakeXmppClient(talk_base::TaskParent* parent)
      : XmppTaskParentInterface(parent) {
  }

  // As XmppTaskParentInterface
  virtual XmppClientInterface* GetClient() {
    return this;
  }

  virtual int ProcessStart() {
    return STATE_RESPONSE;
  }

  // As XmppClientInterface
  virtual XmppEngine::State GetState() const {
    return XmppEngine::STATE_OPEN;
  }

  virtual const Jid& jid() const {
    return jid_;
  }

  virtual std::string NextId() {
    // Implement if needed for tests.
    return "0";
  }

  virtual XmppReturnStatus SendStanza(const XmlElement* stanza) {
    sent_stanzas_.push_back(stanza);
    return XMPP_RETURN_OK;
  }

  const std::vector<const XmlElement*>& sent_stanzas() {
    return sent_stanzas_;
  }

  virtual XmppReturnStatus SendStanzaError(
      const XmlElement * pelOriginal,
      XmppStanzaError code,
      const std::string & text) {
    // Implement if needed for tests.
    return XMPP_RETURN_OK;
  }

  virtual void AddXmppTask(XmppTask* task,
                           XmppEngine::HandlerLevel level) {
    tasks_.push_back(task);
  }

  virtual void RemoveXmppTask(XmppTask* task) {
    std::remove(tasks_.begin(), tasks_.end(), task);
  }

  // As FakeXmppClient
  void set_jid(const Jid& jid) {
    jid_ = jid;
  }

  // Takes ownership of stanza.
  void HandleStanza(XmlElement* stanza) {
    for (std::vector<XmppTask*>::iterator task = tasks_.begin();
         task != tasks_.end(); ++task) {
      if ((*task)->HandleStanza(stanza)) {
        delete stanza;
        return;
      }
    }
    delete stanza;
  }

 private:
  Jid jid_;
  std::vector<XmppTask*> tasks_;
  std::vector<const XmlElement*> sent_stanzas_;
};

}  // namespace buzz

#endif  // TALK_XMPP_FAKEXMPPCLIENT_H_
