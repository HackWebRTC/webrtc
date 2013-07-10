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

#include "talk/xmpp/constants.h"
#include "talk/examples/call/friendinvitesendtask.h"

namespace buzz {

XmppReturnStatus
FriendInviteSendTask::Send(const Jid& user) {
  if (GetState() != STATE_INIT && GetState() != STATE_START)
    return XMPP_RETURN_BADSTATE;

  // Need to first add to roster, then subscribe to presence.
  XmlElement* iq = new XmlElement(QN_IQ);
  iq->AddAttr(QN_TYPE, STR_SET);
  XmlElement* query = new XmlElement(QN_ROSTER_QUERY);
  XmlElement* item = new XmlElement(QN_ROSTER_ITEM);
  item->AddAttr(QN_JID, user.Str());
  item->AddAttr(QN_NAME, user.node());
  query->AddElement(item);
  iq->AddElement(query);
  QueueStanza(iq);

  // Subscribe to presence
  XmlElement* presence = new XmlElement(QN_PRESENCE);
  presence->AddAttr(QN_TO, user.Str());
  presence->AddAttr(QN_TYPE, STR_SUBSCRIBE);
  XmlElement* invitation = new XmlElement(QN_INVITATION);
  invitation->AddAttr(QN_INVITE_MESSAGE,
      "I've been using Google Talk and thought you might like to try it out. "
      "We can use it to call each other for free over the internet. Here's an "
      "invitation to download Google Talk. Give it a try!");
  presence->AddElement(invitation);
  QueueStanza(presence);

  return XMPP_RETURN_OK;
}

int
FriendInviteSendTask::ProcessStart() {
  const XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  if (SendStanza(stanza) != XMPP_RETURN_OK)
    return STATE_ERROR;

  return STATE_START;
}

}
