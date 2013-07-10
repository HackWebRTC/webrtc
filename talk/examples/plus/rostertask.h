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

#ifndef _PHONE_CLIENT_ROSTERTASK_H_
#define _PHONE_CLIENT_ROSTERTASK_H_

#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/app/rosteritem.h"
#include "talk/base/sigslot.h"

namespace buzz {

class RosterTask : public XmppTask {
public:
  RosterTask(Task * parent) :
    XmppTask(parent, XmppEngine::HL_TYPE) {}

  // Roster items removed or updated.  This can come from a push or a get
  sigslot::signal2<const RosterItem &, bool> SignalRosterItemUpdated;
  sigslot::signal1<const RosterItem &> SignalRosterItemRemoved;

  // Subscription messages
  sigslot::signal1<const Jid &> SignalSubscribe;
  sigslot::signal1<const Jid &> SignalUnsubscribe;
  sigslot::signal1<const Jid &> SignalSubscribed;
  sigslot::signal1<const Jid &> SignalUnsubscribed;

  // Roster get
  void RefreshRosterNow();
  sigslot::signal0<> SignalRosterRefreshStarted;
  sigslot::signal0<> SignalRosterRefreshFinished;

  virtual int ProcessStart();

protected:
  void TranslateItems(const XmlElement *rosterQueryResult);

  virtual bool HandleStanza(const XmlElement * stanza);

  // Inner class for doing the roster get
  class RosterGetTask;
  friend class RosterGetTask;
};

}

#endif // _PHONE_CLIENT_ROSTERTASK_H_
