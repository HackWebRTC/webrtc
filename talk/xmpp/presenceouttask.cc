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

#include <time.h>
#include <sstream>
#include "talk/base/stringencode.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/presenceouttask.h"
#include "talk/xmpp/xmppclient.h"

namespace buzz {

XmppReturnStatus
PresenceOutTask::Send(const PresenceStatus & s) {
  if (GetState() != STATE_INIT && GetState() != STATE_START)
    return XMPP_RETURN_BADSTATE;

  XmlElement * presence = TranslateStatus(s);
  QueueStanza(presence);
  delete presence;
  return XMPP_RETURN_OK;
}

XmppReturnStatus
PresenceOutTask::SendDirected(const Jid & j, const PresenceStatus & s) {
  if (GetState() != STATE_INIT && GetState() != STATE_START)
    return XMPP_RETURN_BADSTATE;

  XmlElement * presence = TranslateStatus(s);
  presence->AddAttr(QN_TO, j.Str());
  QueueStanza(presence);
  delete presence;
  return XMPP_RETURN_OK;
}

XmppReturnStatus PresenceOutTask::SendProbe(const Jid & jid) {
  if (GetState() != STATE_INIT && GetState() != STATE_START)
    return XMPP_RETURN_BADSTATE;

  XmlElement * presence = new XmlElement(QN_PRESENCE);
  presence->AddAttr(QN_TO, jid.Str());
  presence->AddAttr(QN_TYPE, "probe");

  QueueStanza(presence);
  delete presence;
  return XMPP_RETURN_OK;
}

int
PresenceOutTask::ProcessStart() {
  const XmlElement * stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  if (SendStanza(stanza) != XMPP_RETURN_OK)
    return STATE_ERROR;

  return STATE_START;
}

XmlElement *
PresenceOutTask::TranslateStatus(const PresenceStatus & s) {
  XmlElement * result = new XmlElement(QN_PRESENCE);
  if (!s.available()) {
    result->AddAttr(QN_TYPE, STR_UNAVAILABLE);
  }
  else {
    if (s.show() != PresenceStatus::SHOW_ONLINE && 
        s.show() != PresenceStatus::SHOW_OFFLINE) {
      result->AddElement(new XmlElement(QN_SHOW));
      switch (s.show()) {
        default:
          result->AddText(STR_SHOW_AWAY, 1);
          break;
        case PresenceStatus::SHOW_XA:
          result->AddText(STR_SHOW_XA, 1);
          break;
        case PresenceStatus::SHOW_DND:
          result->AddText(STR_SHOW_DND, 1);
          break;
        case PresenceStatus::SHOW_CHAT:
          result->AddText(STR_SHOW_CHAT, 1);
          break;
      }
    }

    result->AddElement(new XmlElement(QN_STATUS));
    result->AddText(s.status(), 1);

    if (!s.nick().empty()) {
      result->AddElement(new XmlElement(QN_NICKNAME));
      result->AddText(s.nick(), 1);
    }

    std::string pri;
    talk_base::ToString(s.priority(), &pri);

    result->AddElement(new XmlElement(QN_PRIORITY));
    result->AddText(pri, 1);

    if (s.know_capabilities()) {
      result->AddElement(new XmlElement(QN_CAPS_C, true));
      result->AddAttr(QN_NODE, s.caps_node(), 1);
      result->AddAttr(QN_VER, s.version(), 1);

      std::string caps;
      caps.append(s.voice_capability() ? "voice-v1" : "");
      caps.append(s.pmuc_capability() ? " pmuc-v1" : "");
      caps.append(s.video_capability() ? " video-v1" : "");
      caps.append(s.camera_capability() ? " camera-v1" : "");

      result->AddAttr(QN_EXT, caps, 1);
    }

    // Put the delay mark on the presence according to JEP-0091
    {
      result->AddElement(new XmlElement(kQnDelayX, true));

      // This here is why we *love* the C runtime
      time_t current_time_seconds;
      time(&current_time_seconds);
      struct tm* current_time = gmtime(&current_time_seconds);
      char output[256];
      strftime(output, ARRAY_SIZE(output), "%Y%m%dT%H:%M:%S", current_time);
      result->AddAttr(kQnStamp, output, 1);
    }
  }

  return result;
}


}
