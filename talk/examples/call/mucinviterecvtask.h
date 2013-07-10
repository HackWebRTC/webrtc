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

#ifndef _MUCINVITERECVTASK_H_
#define _MUCINVITERECVTASK_H_

#include <vector>

#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

struct AvailableMediaEntry {
  enum type_t {
    // SIP defines other media types, but these are the only ones we use in
    // multiway jingle.
    // These numbers are important; see .cc file
    TYPE_UNKNOWN = 0, // indicates invalid string
    TYPE_AUDIO = 1,
    TYPE_VIDEO = 2,
  };

  enum status_t {
    // These numbers are important; see .cc file
    STATUS_UNKNOWN = 0, // indicates invalid string
    STATUS_SENDRECV = 1,
    STATUS_SENDONLY = 2,
    STATUS_RECVONLY = 3,
    STATUS_INACTIVE = 4,
  };

  uint32 label;
  type_t type;
  status_t status;

  static const char* TypeAsString(type_t type);
  static const char* StatusAsString(status_t status);
};

class MucInviteRecvTask : public XmppTask {
 public:
  explicit MucInviteRecvTask(XmppTaskParentInterface* parent)
      : XmppTask(parent, XmppEngine::HL_TYPE) {}
  virtual int ProcessStart();

  // First arg is inviter's JID; second is MUC's JID.
  sigslot::signal3<const Jid&, const Jid&, const std::vector<AvailableMediaEntry>& > SignalInviteReceived;

 protected:
  virtual bool HandleStanza(const XmlElement* stanza);

};

}

#endif
