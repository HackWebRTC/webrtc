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

#ifndef TALK_XMPP_PUBSUBTASKS_H_
#define TALK_XMPP_PUBSUBTASKS_H_

#include <vector>

#include "talk/base/sigslot.h"
#include "talk/xmpp/iqtask.h"
#include "talk/xmpp/receivetask.h"

namespace buzz {

// A PubSub itemid + payload.  Useful for signaling items.
struct PubSubItem {
  std::string itemid;
  // The entire <item>, owned by the stanza handler.  To keep a
  // reference after handling, make a copy.
  const XmlElement* elem;
};

// An IqTask which gets a <pubsub><items> for a particular jid and
// node, parses the items in the response and signals the items.
class PubSubRequestTask : public IqTask {
 public:
  PubSubRequestTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node);

  sigslot::signal2<PubSubRequestTask*,
                   const std::vector<PubSubItem>&> SignalResult;
  // SignalError inherited by IqTask.
 private:
  virtual void HandleResult(const XmlElement* stanza);
};

// A ReceiveTask which listens for <event><items> of a particular
// pubsub JID and node and then signals them items.
class PubSubReceiveTask : public ReceiveTask {
 public:
  PubSubReceiveTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node)
      : ReceiveTask(parent),
        pubsubjid_(pubsubjid),
        node_(node) {
  }

  sigslot::signal2<PubSubReceiveTask*,
                   const std::vector<PubSubItem>&> SignalUpdate;

 protected:
  virtual bool WantsStanza(const XmlElement* stanza);
  virtual void ReceiveStanza(const XmlElement* stanza);

 private:
  Jid pubsubjid_;
  std::string node_;
};

// An IqTask which publishes a <pubsub><publish><item> to a particular
// pubsub jid and node.
class PubSubPublishTask : public IqTask {
 public:
  // Takes ownership of children
  PubSubPublishTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node,
                    const std::string& itemid,
                    const std::vector<XmlElement*>& children);

  const std::string& itemid() const { return itemid_; }

  sigslot::signal1<PubSubPublishTask*> SignalResult;

 private:
  // SignalError inherited by IqTask.
  virtual void HandleResult(const XmlElement* stanza);

  std::string itemid_;
};

// An IqTask which publishes a <pubsub><publish><retract> to a particular
// pubsub jid and node.
class PubSubRetractTask : public IqTask {
 public:
  PubSubRetractTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node,
                    const std::string& itemid);

  const std::string& itemid() const { return itemid_; }

  sigslot::signal1<PubSubRetractTask*> SignalResult;

 private:
  // SignalError inherited by IqTask.
  virtual void HandleResult(const XmlElement* stanza);

  std::string itemid_;
};

}  // namespace buzz

#endif  // TALK_XMPP_PUBSUBTASKS_H_
