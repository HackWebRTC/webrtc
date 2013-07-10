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

#ifndef _PRESENCEPUSHTASK_H_
#define _PRESENCEPUSHTASK_H_

#include <vector>

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/xmpp/presencestatus.h"
#include "talk/base/sigslot.h"
#include "talk/examples/call/callclient.h"

namespace buzz {

class PresencePushTask : public XmppTask {
 public:
  PresencePushTask(XmppTaskParentInterface* parent, CallClient* client)
    : XmppTask(parent, XmppEngine::HL_TYPE),
      client_(client) {}
  virtual int ProcessStart();

  sigslot::signal1<const PresenceStatus&> SignalStatusUpdate;
  sigslot::signal1<const Jid&> SignalMucJoined;
  sigslot::signal2<const Jid&, int> SignalMucLeft;
  sigslot::signal2<const Jid&, const MucPresenceStatus&> SignalMucStatusUpdate;

 protected:
  virtual bool HandleStanza(const XmlElement * stanza);
  void HandlePresence(const Jid& from, const XmlElement * stanza);
  void HandleMucPresence(buzz::Muc* muc,
                         const Jid& from, const XmlElement * stanza);
  static void FillStatus(const Jid& from, const XmlElement * stanza,
                         PresenceStatus* status);
  static void FillMucStatus(const Jid& from, const XmlElement * stanza,
                            MucPresenceStatus* status);

 private:
  CallClient* client_;
};


}

#endif
