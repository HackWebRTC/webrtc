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

#include "talk/base/stringencode.h"
#include "presencepushtask.h"
#include "talk/xmpp/constants.h"
#include <sstream>


namespace buzz {

// string helper functions -----------------------------------------------------

static bool
IsXmlSpace(int ch) {
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

static bool
ListContainsToken(const std::string & list, const std::string & token) {
  size_t i = list.find(token);
  if (i == std::string::npos || token.empty())
    return false;
  bool boundary_before = (i == 0 || IsXmlSpace(list[i - 1]));
  bool boundary_after = (i == list.length() - token.length() || IsXmlSpace(list[i + token.length()]));
  return boundary_before && boundary_after;
}


bool
PresencePushTask::HandleStanza(const XmlElement * stanza) {
  if (stanza->Name() != QN_PRESENCE)
    return false;
  if (stanza->HasAttr(QN_TYPE) && stanza->Attr(QN_TYPE) != STR_UNAVAILABLE) {
    if (stanza->Attr(QN_TYPE) == STR_ERROR) {
      // Pass on the error.
      const XmlElement* error_xml_elem = stanza->FirstNamed(QN_ERROR);
      if (!error_xml_elem) {
        return false;
      }
      SignalStatusError(*error_xml_elem);
      return true;
    }
  }
  QueueStanza(stanza);
  return true;
}

static bool IsUtf8FirstByte(int c) {
  return (((c)&0x80)==0) || // is single byte
    ((unsigned char)((c)-0xc0)<0x3e); // or is lead byte
}

int
PresencePushTask::ProcessStart() {
  const XmlElement * stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;
  Status s;

  s.set_jid(Jid(stanza->Attr(QN_FROM)));

  if (stanza->Attr(QN_TYPE) == STR_UNAVAILABLE) {
    s.set_available(false);
    SignalStatusUpdate(s);
  }
  else {
    s.set_available(true);
    const XmlElement * status = stanza->FirstNamed(QN_STATUS);
    if (status != NULL) {
      s.set_status(status->BodyText());

      // Truncate status messages longer than 300 bytes
      if (s.status().length() > 300) {
        size_t len = 300;

        // Be careful not to split legal utf-8 chars in half
        while (!IsUtf8FirstByte(s.status()[len]) && len > 0) {
          len -= 1;
        }
        std::string truncated(s.status(), 0, len);
        s.set_status(truncated);
      }
    }

    const XmlElement * priority = stanza->FirstNamed(QN_PRIORITY);
    if (priority != NULL) {
      int pri;
      if (talk_base::FromString(priority->BodyText(), &pri)) {
        s.set_priority(pri);
      }
    }

    const XmlElement * show = stanza->FirstNamed(QN_SHOW);
    if (show == NULL || show->FirstChild() == NULL) {
      s.set_show(Status::SHOW_ONLINE);
    }
    else {
      if (show->BodyText() == "away") {
        s.set_show(Status::SHOW_AWAY);
      }
      else if (show->BodyText() == "xa") {
        s.set_show(Status::SHOW_XA);
      }
      else if (show->BodyText() == "dnd") {
        s.set_show(Status::SHOW_DND);
      }
      else if (show->BodyText() == "chat") {
        s.set_show(Status::SHOW_CHAT);
      }
      else {
        s.set_show(Status::SHOW_ONLINE);
      }
    }

    const XmlElement * caps = stanza->FirstNamed(QN_CAPS_C);
    if (caps != NULL) {
      std::string node = caps->Attr(QN_NODE);
      std::string ver = caps->Attr(QN_VER);
      std::string exts = caps->Attr(QN_EXT);

      s.set_know_capabilities(true);
      std::string capability;
      std::stringstream ss(exts);
      while (ss >> capability) {
        s.AddCapability(capability);
      }

      s->set_caps_node(node);
      s->set_version(ver);
    }

    const XmlElement* delay = stanza->FirstNamed(kQnDelayX);
    if (delay != NULL) {
      // Ideally we would parse this according to the Psuedo ISO-8601 rules
      // that are laid out in JEP-0082:
      // http://www.jabber.org/jeps/jep-0082.html
      std::string stamp = delay->Attr(kQnStamp);
      s.set_sent_time(stamp);
    }

    const XmlElement *nick = stanza->FirstNamed(kQnNickname);
    if (nick) {
      std::string user_nick = nick->BodyText();
      s.set_user_nick(user_nick);
    }

    const XmlElement *plugin = stanza->FirstNamed(QN_PLUGIN);
    if (plugin) {
      const XmlElement *api_cap = plugin->FirstNamed(QN_CAPABILITY);
      if (api_cap) {
        const std::string &api_capability = api_cap->BodyText();
        s.set_api_capability(api_capability);
      }
      const XmlElement *api_msg = plugin->FirstNamed(QN_DATA);
      if (api_msg) {
        const std::string &api_message = api_msg->BodyText();
        s.set_api_message(api_message);
      }
    }

    const XmlElement* data_x = stanza->FirstNamed(QN_MUC_USER_X);
    if (data_x != NULL) {
      const XmlElement* item = data_x->FirstNamed(QN_MUC_USER_ITEM);
      if (item != NULL) {
        s.set_muc_role(item->Attr(QN_ROLE));
      }
    }

    SignalStatusUpdate(s);
  }

  return STATE_START;
}


}
