/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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

#include "talk/examples/chat/textchatsendtask.h"

#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppclient.h"

namespace buzz {
TextChatSendTask::TextChatSendTask(XmppTaskParentInterface* parent)
  : XmppTask(parent) {
}

TextChatSendTask::~TextChatSendTask() {
  Stop();
}

XmppReturnStatus TextChatSendTask::Send(const Jid& to,
                                        const std::string& textmessage) {
  // Make sure we are actually connected.
  if (GetState() != STATE_INIT && GetState() != STATE_START) {
    return XMPP_RETURN_BADSTATE;
  }

  // Put together the chat stanza...
  XmlElement* message_stanza = new XmlElement(QN_MESSAGE);

  // ... and specify the required attributes...
  message_stanza->AddAttr(QN_TO, to.Str());
  message_stanza->AddAttr(QN_TYPE, "chat");
  message_stanza->AddAttr(QN_LANG, "en");

  // ... and fill out the body.
  XmlElement* message_body = new XmlElement(QN_BODY);
  message_body->AddText(textmessage);
  message_stanza->AddElement(message_body);

  // Now queue it up.
  QueueStanza(message_stanza);

  return XMPP_RETURN_OK;
}

int TextChatSendTask::ProcessStart() {
  const XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }

  if (SendStanza(stanza) != XMPP_RETURN_OK) {
    return STATE_ERROR;
  }

  return STATE_START;
}

}  // namespace buzz
