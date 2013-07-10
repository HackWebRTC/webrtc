/*
 * libjingle
 * Copyright 2004--2012, Google Inc.
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

#ifndef THIRD_PARTY_LIBJINGLE_FILES_TALK_XMPP_PRESENCERECEIVETASK_H_
#define THIRD_PARTY_LIBJINGLE_FILES_TALK_XMPP_PRESENCERECEIVETASK_H_

#include "talk/base/sigslot.h"

#include "talk/xmpp/presencestatus.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

// A task to receive presence status callbacks from the XMPP server.
class PresenceReceiveTask : public XmppTask {
 public:
  // Arguments:
  //   parent a reference to task interface associated withe the XMPP client.
  explicit PresenceReceiveTask(XmppTaskParentInterface* parent);

  // Shuts down the thread associated with this task.
  virtual ~PresenceReceiveTask();

  // Starts pulling queued status messages and dispatching them to the
  // PresenceUpdate() callback.
  virtual int ProcessStart();

  // Slot for presence message callbacks
  sigslot::signal1<const PresenceStatus&> PresenceUpdate;

 protected:
  // Called by the XMPP engine when presence stanzas are received from the
  // server.
  virtual bool HandleStanza(const XmlElement * stanza);

 private:
  // Handles presence stanzas by converting the data to PresenceStatus
  // objects and passing those along to the SignalStatusUpadate() callback.
  void HandlePresence(const Jid& from, const XmlElement * stanza);

  // Extracts presence information for the presence stanza sent form the
  // server.
  static void DecodeStatus(const Jid& from, const XmlElement * stanza,
                           PresenceStatus* status);
};

} // namespace buzz

#endif // THIRD_PARTY_LIBJINGLE_FILES_TALK_XMPP_PRESENCERECEIVETASK_H_
