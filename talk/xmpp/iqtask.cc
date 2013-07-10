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

#include "talk/xmpp/iqtask.h"

#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/constants.h"

namespace buzz {

static const int kDefaultIqTimeoutSecs = 15;

IqTask::IqTask(XmppTaskParentInterface* parent,
               const std::string& verb,
               const buzz::Jid& to,
               buzz::XmlElement* el)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      to_(to),
      stanza_(MakeIq(verb, to_, task_id())) {
  stanza_->AddElement(el);
  set_timeout_seconds(kDefaultIqTimeoutSecs);
}

int IqTask::ProcessStart() {
  buzz::XmppReturnStatus ret = SendStanza(stanza_.get());
  // TODO: HandleError(NULL) if SendStanza fails?
  return (ret == buzz::XMPP_RETURN_OK) ? STATE_RESPONSE : STATE_ERROR;
}

bool IqTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, to_, task_id()))
    return false;

  if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_RESULT &&
      stanza->Attr(buzz::QN_TYPE) != buzz::STR_ERROR) {
    return false;
  }

  QueueStanza(stanza);
  return true;
}

int IqTask::ProcessResponse() {
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  bool success = (stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT);
  if (success) {
    HandleResult(stanza);
  } else {
    SignalError(this, stanza->FirstNamed(QN_ERROR));
  }
  return STATE_DONE;
}

int IqTask::OnTimeout() {
  SignalError(this, NULL);
  return XmppTask::OnTimeout();
}

}  // namespace buzz
