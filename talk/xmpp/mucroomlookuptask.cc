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

#include "talk/xmpp/mucroomlookuptask.h"

#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/constants.h"


namespace buzz {

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForRoomName(XmppTaskParentInterface* parent,
                                     const Jid& lookup_server_jid,
                                     const std::string& room_name,
                                     const std::string& room_domain) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeNameQuery(room_name, room_domain));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForRoomJid(XmppTaskParentInterface* parent,
                                              const Jid& lookup_server_jid,
                                              const Jid& room_jid) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeJidQuery(room_jid));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForHangoutId(XmppTaskParentInterface* parent,
                                                const Jid& lookup_server_jid,
                                                const std::string& hangout_id) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeHangoutIdQuery(hangout_id));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForExternalId(
    XmppTaskParentInterface* parent,
    const Jid& lookup_server_jid,
    const std::string& external_id,
    const std::string& type) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeExternalIdQuery(external_id, type));
}

MucRoomLookupTask::MucRoomLookupTask(XmppTaskParentInterface* parent,
                                     const Jid& lookup_server_jid,
                                     XmlElement* query)
    : IqTask(parent, STR_SET, lookup_server_jid, query) {
}

XmlElement* MucRoomLookupTask::MakeNameQuery(
    const std::string& room_name, const std::string& room_domain) {
  XmlElement* name_elem = new XmlElement(QN_SEARCH_ROOM_NAME, false);
  name_elem->SetBodyText(room_name);

  XmlElement* domain_elem = new XmlElement(QN_SEARCH_ROOM_DOMAIN, false);
  domain_elem->SetBodyText(room_domain);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY, true);
  query->AddElement(name_elem);
  query->AddElement(domain_elem);
  return query;
}

XmlElement* MucRoomLookupTask::MakeJidQuery(const Jid& room_jid) {
  XmlElement* jid_elem = new XmlElement(QN_SEARCH_ROOM_JID);
  jid_elem->SetBodyText(room_jid.Str());

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY);
  query->AddElement(jid_elem);
  return query;
}

XmlElement* MucRoomLookupTask::MakeExternalIdQuery(
    const std::string& external_id, const std::string& type) {
  XmlElement* external_id_elem = new XmlElement(QN_SEARCH_EXTERNAL_ID);
  external_id_elem->SetAttr(QN_TYPE, type);
  external_id_elem->SetBodyText(external_id);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY);
  query->AddElement(external_id_elem);
  return query;
}

// Construct a stanza to lookup the muc jid for a given hangout id. eg:
//
// <query xmlns="jabber:iq:search">
//   <hangout-id>0b48ad092c893a53b7bfc87422caf38e93978798e</hangout-id>
// </query>
XmlElement* MucRoomLookupTask::MakeHangoutIdQuery(
    const std::string& hangout_id) {
  XmlElement* hangout_id_elem = new XmlElement(QN_SEARCH_HANGOUT_ID, false);
  hangout_id_elem->SetBodyText(hangout_id);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY, true);
  query->AddElement(hangout_id_elem);
  return query;
}

// Handle a response like the following:
//
// <query xmlns="jabber:iq:search">
//   <item jid="muvc-private-chat-guid@groupchat.google.com">
//     <room-name>0b48ad092c893a53b7bfc87422caf38e93978798e</room-name>
//     <room-domain>hangout.google.com</room-domain>
//   </item>
// </query>
void MucRoomLookupTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query_elem = stanza->FirstNamed(QN_SEARCH_QUERY);
  if (query_elem == NULL) {
    SignalError(this, stanza);
    return;
  }

  const XmlElement* item_elem = query_elem->FirstNamed(QN_SEARCH_ITEM);
  if (item_elem == NULL) {
    SignalError(this, stanza);
    return;
  }

  MucRoomInfo room;
  room.jid = Jid(item_elem->Attr(buzz::QN_JID));
  if (!room.jid.IsValid()) {
    SignalError(this, stanza);
    return;
  }

  const XmlElement* room_name_elem =
      item_elem->FirstNamed(QN_SEARCH_ROOM_NAME);
  if (room_name_elem != NULL) {
    room.name = room_name_elem->BodyText();
  }

  const XmlElement* room_domain_elem =
      item_elem->FirstNamed(QN_SEARCH_ROOM_DOMAIN);
  if (room_domain_elem != NULL) {
    room.domain = room_domain_elem->BodyText();
  }

  const XmlElement* hangout_id_elem =
      item_elem->FirstNamed(QN_SEARCH_HANGOUT_ID);
  if (hangout_id_elem != NULL) {
    room.hangout_id = hangout_id_elem->BodyText();
  }

  SignalResult(this, room);
}

}  // namespace buzz
