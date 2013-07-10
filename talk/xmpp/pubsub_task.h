/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_XMPP_PUBSUB_TASK_H_
#define TALK_XMPP_PUBSUB_TASK_H_

#include <map>
#include <string>
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

// Base class to help write pubsub tasks.
// In ProcessStart call SubscribeNode with namespaces of interest along with
// NodeHandlers.
// When pubsub notifications arrive and matches the namespace, the NodeHandlers
// will be called back.
class PubsubTask : public buzz::XmppTask {
 public:
  virtual ~PubsubTask();

 protected:
  typedef void (PubsubTask::*NodeHandler)(const buzz::XmlElement* node);

  PubsubTask(XmppTaskParentInterface* parent, const buzz::Jid& pubsub_node_jid);

  virtual bool HandleStanza(const buzz::XmlElement* stanza);
  virtual int ProcessResponse();

  bool SubscribeToNode(const std::string& pubsub_node, NodeHandler handler);
  void UnsubscribeFromNode(const std::string& pubsub_node);

  // Called when there is an error. Derived class can do what it needs to.
  virtual void OnPubsubError(const buzz::XmlElement* error_stanza);

 private:
  typedef std::map<std::string, NodeHandler> NodeSubscriptions;

  void HandlePubsubIqGetResponse(const buzz::XmlElement* pubsub_iq_response);
  void HandlePubsubEventMessage(const buzz::XmlElement* pubsub_event_message);
  void HandlePubsubItems(const buzz::XmlElement* items);

  buzz::Jid pubsub_node_jid_;
  NodeSubscriptions subscribed_nodes_;
};

}  // namespace buzz

#endif // TALK_XMPP_PUBSUB_TASK_H_
