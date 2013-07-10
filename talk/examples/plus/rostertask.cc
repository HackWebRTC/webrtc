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

#include "rostertask.h"
#include "talk/xmpp/constants.h"
#include "talk/base/stream.h"

#undef WIN32
#ifdef WIN32
#include "talk/app/win32/offlineroster.h"
#endif

namespace buzz {

class RosterTask::RosterGetTask : public XmppTask {
public:
  RosterGetTask(Task * parent) : XmppTask(parent, XmppEngine::HL_SINGLE),
    done_(false) {}

  virtual int ProcessStart();
  virtual int ProcessResponse();

protected:
  virtual bool HandleStanza(const XmlElement * stanza);

  bool done_;
};

//==============================================================================
// RosterTask
//==============================================================================
void RosterTask::RefreshRosterNow() {
  RosterGetTask* get_task = new RosterGetTask(this);
  ResumeTimeout();
  get_task->Start();
}

void RosterTask::TranslateItems(const XmlElement * rosterQueryResult) {
#if defined(FEATURE_ENABLE_PSTN)
#ifdef WIN32
  // We build up a list of contacts which have had information persisted offline.
  // we'll remove items from this list if we get a buzz::SUBSCRIBE_REMOVE
  // subscription.  After updating all the items from the server, we'll then
  // update (and merge) any roster items left in our map of offline items
  XmlElement *el_local = OfflineRoster::RetrieveOfflineRoster(GetClient()->jid());
  std::map<buzz::Jid, RosterItem> jid_to_item;
  if (el_local) {
    for (XmlElement *el_item = el_local->FirstNamed(QN_ROSTER_ITEM);
         el_item != NULL;
         el_item = el_item->NextNamed(QN_ROSTER_ITEM)) {
      RosterItem roster_item;
      roster_item.FromXml(el_item);

      jid_to_item[roster_item.jid()] = roster_item;
    }
  }
#endif // WIN32
#endif // FEATURE_ENABLE_PSTN

  const XmlElement * xml_item;
  for (xml_item = rosterQueryResult->FirstNamed(QN_ROSTER_ITEM);
       xml_item != NULL; xml_item = xml_item->NextNamed(QN_ROSTER_ITEM)) {
    RosterItem roster_item;
    roster_item.FromXml(xml_item);

    if (roster_item.subscription() == buzz::SUBSCRIBE_REMOVE) {
      SignalRosterItemRemoved(roster_item);

#if defined(FEATURE_ENABLE_PSTN)
#ifdef WIN32
      std::map<buzz::Jid, RosterItem>::iterator it =
        jid_to_item.find(roster_item.jid());

      if (it != jid_to_item.end())
        jid_to_item.erase(it);
#endif
#endif
    } else {
      SignalRosterItemUpdated(roster_item, false);
    }
  }

#if defined(FEATURE_ENABLE_PSTN)
#ifdef WIN32
  for (std::map<buzz::Jid, RosterItem>::iterator it = jid_to_item.begin();
       it != jid_to_item.end(); ++it) {
    SignalRosterItemUpdated(it->second, true);
  }
#endif
#endif
}

int RosterTask::ProcessStart() {
  const XmlElement * stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  if (stanza->Name() == QN_IQ) {
    SuspendTimeout();
    bool result = (stanza->Attr(QN_TYPE) == STR_RESULT);
    if (result)
      SignalRosterRefreshStarted();

    TranslateItems(stanza->FirstNamed(QN_ROSTER_QUERY));

    if (result)
      SignalRosterRefreshFinished();
  } else if (stanza->Name() == QN_PRESENCE) {
    Jid jid(stanza->Attr(QN_FROM));
    std::string type = stanza->Attr(QN_TYPE);
    if (type == "subscribe")
      SignalSubscribe(jid);
    else if (type == "unsubscribe")
      SignalUnsubscribe(jid);
    else if (type == "subscribed")
      SignalSubscribed(jid);
    else if (type == "unsubscribed")
      SignalUnsubscribed(jid);
  }

  return STATE_START;
}

bool RosterTask::HandleStanza(const XmlElement * stanza) {
  if (!MatchRequestIq(stanza, STR_SET, QN_ROSTER_QUERY)) {
    // Not a roster IQ.  Look for a presence instead
    if (stanza->Name() != QN_PRESENCE)
      return false;
    if (!stanza->HasAttr(QN_TYPE))
      return false;
    std::string type = stanza->Attr(QN_TYPE);
    if (type == "subscribe" || type == "unsubscribe" ||
        type == "subscribed" || type == "unsubscribed") {
      QueueStanza(stanza);
      return true;
    }
    return false;
  }

  // only respect roster push from the server
  Jid from(stanza->Attr(QN_FROM));
  if (from != JID_EMPTY &&
      !from.BareEquals(GetClient()->jid()) &&
      from != Jid(GetClient()->jid().domain()))
    return false;

  XmlElement * result = MakeIqResult(stanza);
  result->AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  SendStanza(result);

  QueueStanza(stanza);
  return true;
}


//==============================================================================
// RosterTask::RosterGetTask
//==============================================================================
int RosterTask::RosterGetTask::ProcessStart() {
  talk_base::scoped_ptr<XmlElement> get(MakeIq(STR_GET, JID_EMPTY, task_id()));
  get->AddElement(new XmlElement(QN_ROSTER_QUERY, true));
  get->AddAttr(QN_XMLNS_GR, NS_GR, 1);
  get->AddAttr(QN_GR_EXT, "2", 1);
  get->AddAttr(QN_GR_INCLUDE, "all", 1);
  if (SendStanza(get.get()) != XMPP_RETURN_OK) {
    return STATE_ERROR;
  }
  return STATE_RESPONSE;
}

int RosterTask::RosterGetTask::ProcessResponse() {
  if (done_)
    return STATE_DONE;
  return STATE_BLOCKED;
}

bool RosterTask::RosterGetTask::HandleStanza(const XmlElement * stanza) {
  if (!MatchResponseIq(stanza, JID_EMPTY, task_id()))
    return false;

  if (stanza->Attr(QN_TYPE) != STR_RESULT)
    return false;

  // Queue the stanza with the parent so these don't get handled out of order
  RosterTask* parent = static_cast<RosterTask*>(GetParent());
  parent->QueueStanza(stanza);

  // Wake ourselves so we can go into the done state
  done_ = true;
  Wake();
  return true;
}

}
