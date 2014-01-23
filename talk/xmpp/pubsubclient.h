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

#ifndef TALK_XMPP_PUBSUBCLIENT_H_
#define TALK_XMPP_PUBSUBCLIENT_H_

#include <string>
#include <vector>

#include "talk/base/sigslot.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/base/task.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/pubsubtasks.h"

// Easy to use clients built on top of the tasks for XEP-0060
// (http://xmpp.org/extensions/xep-0060.html).

namespace buzz {

class Jid;
class XmlElement;
class XmppTaskParentInterface;

// An easy-to-use pubsub client that handles the three tasks of
// getting, publishing, and listening for updates.  Tied to a specific
// pubsub jid and node.  All you have to do is RequestItems, listen
// for SignalItems and PublishItems.
class PubSubClient : public sigslot::has_slots<> {
 public:
  PubSubClient(XmppTaskParentInterface* parent,
               const Jid& pubsubjid,
               const std::string& node)
    : parent_(parent),
      pubsubjid_(pubsubjid),
      node_(node) {}

  const std::string& node() const { return node_; }

  // Requests the <pubsub><items>, which will be returned via
  // SignalItems, or SignalRequestError if there is a failure.  Should
  // auto-subscribe.
  void RequestItems();
  // Fired when either <pubsub><items> are returned or when
  // <event><items> are received.
  sigslot::signal2<PubSubClient*,
                   const std::vector<PubSubItem>&> SignalItems;
  // Signal (this, error stanza)
  sigslot::signal2<PubSubClient*,
                   const XmlElement*> SignalRequestError;
  // Signal (this, task_id, item, error stanza)
  sigslot::signal4<PubSubClient*,
                   const std::string&,
                   const XmlElement*,
                   const XmlElement*> SignalPublishError;
  // Signal (this, task_id, item)
  sigslot::signal3<PubSubClient*,
                   const std::string&,
                   const XmlElement*> SignalPublishResult;
  // Signal (this, task_id, error stanza)
  sigslot::signal3<PubSubClient*,
                   const std::string&,
                   const XmlElement*> SignalRetractError;
  // Signal (this, task_id)
  sigslot::signal2<PubSubClient*,
                   const std::string&> SignalRetractResult;

  // Publish an item.  Takes ownership of payload.
  void PublishItem(const std::string& itemid,
                   XmlElement* payload,
                   std::string* task_id_out);
  // Publish an item.  Takes ownership of children.
  void PublishItem(const std::string& itemid,
                   const std::vector<XmlElement*>& children,
                   std::string* task_id_out);
  // Retract (delete) an item.
  void RetractItem(const std::string& itemid,
                   std::string* task_id_out);

  // Get the publisher nick if it exists from the pubsub item.
  const std::string GetPublisherNickFromPubSubItem(const XmlElement* item_elem);

 private:
  void OnRequestError(IqTask* task,
                      const XmlElement* stanza);
  void OnRequestResult(PubSubRequestTask* task,
                       const std::vector<PubSubItem>& items);
  void OnReceiveUpdate(PubSubReceiveTask* task,
                       const std::vector<PubSubItem>& items);
  void OnPublishResult(PubSubPublishTask* task);
  void OnPublishError(IqTask* task,
                      const XmlElement* stanza);
  void OnRetractResult(PubSubRetractTask* task);
  void OnRetractError(IqTask* task,
                      const XmlElement* stanza);

  XmppTaskParentInterface* parent_;
  Jid pubsubjid_;
  std::string node_;
};

}  // namespace buzz

#endif  // TALK_XMPP_PUBSUBCLIENT_H_
