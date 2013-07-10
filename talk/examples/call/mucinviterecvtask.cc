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
#include "talk/examples/call/mucinviterecvtask.h"

namespace buzz {

const char* types[] = {
  "unknown",
  "audio",
  "video",
};

const char* statuses[] = {
  "unknown",
  "sendrecv",
  "sendonly",
  "recvonly",
  "inactive",
};

const char*
AvailableMediaEntry::TypeAsString(type_t type) {
  // The values of the constants have been chosen such that this is correct.
  return types[type];
}

const char*
AvailableMediaEntry::StatusAsString(status_t status) {
  // The values of the constants have been chosen such that this is correct.
  return statuses[status];
}

int bodytext_to_array_pos(const XmlElement* elem, const char* array[],
    int len, int defval = -1) {
  if (elem) {
    const std::string& body(elem->BodyText());
    for (int i = 0; i < len; ++i) {
      if (body == array[i]) {
        // Found it.
        return i;
      }
    }
  }
  // If we get here, it's not any value in the array.
  return defval;
}

bool
MucInviteRecvTask::HandleStanza(const XmlElement* stanza) {
  // Figuring out that we want to handle this is a lot of the work of
  // actually handling it, so we handle it right here instead of queueing it.
  const XmlElement* xstanza;
  const XmlElement* invite;
  if (stanza->Name() != QN_MESSAGE) return false;
  xstanza = stanza->FirstNamed(QN_MUC_USER_X);
  if (!xstanza) return false;
  invite = xstanza->FirstNamed(QN_MUC_USER_INVITE);
  if (!invite) return false;
  // Else it's an invite and we definitely want to handle it. Parse the
  // available-media, if any.
  std::vector<AvailableMediaEntry> v;
  const XmlElement* avail =
    invite->FirstNamed(QN_GOOGLE_MUC_USER_AVAILABLE_MEDIA);
  if (avail) {
    for (const XmlElement* entry = avail->FirstNamed(QN_GOOGLE_MUC_USER_ENTRY);
        entry;
        entry = entry->NextNamed(QN_GOOGLE_MUC_USER_ENTRY)) {
      AvailableMediaEntry tmp;
      // In the interest of debugging, we accept as much valid-looking data
      // as we can.
      tmp.label = atoi(entry->Attr(QN_LABEL).c_str());
      tmp.type = static_cast<AvailableMediaEntry::type_t>(
          bodytext_to_array_pos(
              entry->FirstNamed(QN_GOOGLE_MUC_USER_TYPE),
              types,
              sizeof(types)/sizeof(const char*),
              AvailableMediaEntry::TYPE_UNKNOWN));
      tmp.status = static_cast<AvailableMediaEntry::status_t>(
          bodytext_to_array_pos(
              entry->FirstNamed(QN_GOOGLE_MUC_USER_STATUS),
              statuses,
              sizeof(statuses)/sizeof(const char*),
              AvailableMediaEntry::STATUS_UNKNOWN));
      v.push_back(tmp);
    }
  }
  SignalInviteReceived(Jid(invite->Attr(QN_FROM)), Jid(stanza->Attr(QN_FROM)),
      v);
  return true;
}

int
MucInviteRecvTask::ProcessStart() {
  // We never queue anything so we are always blocked.
  return STATE_BLOCKED;
}

}
