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

#include "talk/examples/call/mucinvitesendtask.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppclient.h"

namespace buzz {

XmppReturnStatus
MucInviteSendTask::Send(const Jid& to, const Jid& invitee) {
  if (GetState() != STATE_INIT && GetState() != STATE_START)
    return XMPP_RETURN_BADSTATE;

  XmlElement* message = new XmlElement(QN_MESSAGE);
  message->AddAttr(QN_TO, to.Str());
  XmlElement* xstanza = new XmlElement(QN_MUC_USER_X);
  XmlElement* invite = new XmlElement(QN_MUC_USER_INVITE);
  invite->AddAttr(QN_TO, invitee.Str());
  xstanza->AddElement(invite);
  message->AddElement(xstanza);

  QueueStanza(message);
  return XMPP_RETURN_OK;
}

int
MucInviteSendTask::ProcessStart() {
  const XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  if (SendStanza(stanza) != XMPP_RETURN_OK)
    return STATE_ERROR;

  return STATE_START;
}

}
