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

#include "talk/xmpp/pubsubclient.h"

#include <string>
#include <vector>

#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/pubsubtasks.h"

namespace buzz {

void PubSubClient::RequestItems() {
  PubSubRequestTask* request_task =
      new PubSubRequestTask(parent_, pubsubjid_, node_);
  request_task->SignalResult.connect(this, &PubSubClient::OnRequestResult);
  request_task->SignalError.connect(this, &PubSubClient::OnRequestError);

  PubSubReceiveTask* receive_task =
      new PubSubReceiveTask(parent_, pubsubjid_, node_);
  receive_task->SignalUpdate.connect(this, &PubSubClient::OnReceiveUpdate);

  receive_task->Start();
  request_task->Start();
}

void PubSubClient::PublishItem(
    const std::string& itemid, XmlElement* payload, std::string* task_id_out) {
  std::vector<XmlElement*> children;
  children.push_back(payload);
  PublishItem(itemid, children, task_id_out);
}

void PubSubClient::PublishItem(
    const std::string& itemid, const std::vector<XmlElement*>& children,
    std::string* task_id_out) {
  PubSubPublishTask* publish_task =
      new PubSubPublishTask(parent_, pubsubjid_, node_, itemid, children);
  publish_task->SignalError.connect(this, &PubSubClient::OnPublishError);
  publish_task->SignalResult.connect(this, &PubSubClient::OnPublishResult);
  publish_task->Start();
  if (task_id_out) {
    *task_id_out = publish_task->task_id();
  }
}

void PubSubClient::RetractItem(
    const std::string& itemid, std::string* task_id_out) {
  PubSubRetractTask* retract_task =
      new PubSubRetractTask(parent_, pubsubjid_, node_, itemid);
  retract_task->SignalError.connect(this, &PubSubClient::OnRetractError);
  retract_task->SignalResult.connect(this, &PubSubClient::OnRetractResult);
  retract_task->Start();
  if (task_id_out) {
    *task_id_out = retract_task->task_id();
  }
}

void PubSubClient::OnRequestResult(PubSubRequestTask* task,
                                   const std::vector<PubSubItem>& items) {
  SignalItems(this, items);
}

void PubSubClient::OnRequestError(IqTask* task,
                                  const XmlElement* stanza) {
  SignalRequestError(this, stanza);
}

void PubSubClient::OnReceiveUpdate(PubSubReceiveTask* task,
                                   const std::vector<PubSubItem>& items) {
  SignalItems(this, items);
}

const XmlElement* GetItemFromStanza(const XmlElement* stanza) {
  if (stanza != NULL) {
    const XmlElement* pubsub = stanza->FirstNamed(QN_PUBSUB);
    if (pubsub != NULL) {
      const XmlElement* publish = pubsub->FirstNamed(QN_PUBSUB_PUBLISH);
      if (publish != NULL) {
        return publish->FirstNamed(QN_PUBSUB_ITEM);
      }
    }
  }
  return NULL;
}

void PubSubClient::OnPublishResult(PubSubPublishTask* task) {
  const XmlElement* item = GetItemFromStanza(task->stanza());
  SignalPublishResult(this, task->task_id(), item);
}

void PubSubClient::OnPublishError(IqTask* task,
                                  const XmlElement* error_stanza) {
  PubSubPublishTask* publish_task =
      static_cast<PubSubPublishTask*>(task);
  const XmlElement* item = GetItemFromStanza(publish_task->stanza());
  SignalPublishError(this, publish_task->task_id(), item, error_stanza);
}

void PubSubClient::OnRetractResult(PubSubRetractTask* task) {
  SignalRetractResult(this, task->task_id());
}

void PubSubClient::OnRetractError(IqTask* task,
                                  const XmlElement* stanza) {
  PubSubRetractTask* retract_task =
      static_cast<PubSubRetractTask*>(task);
  SignalRetractError(this, retract_task->task_id(), stanza);
}


const std::string PubSubClient::GetPublisherNickFromPubSubItem(
    const XmlElement* item_elem) {
  if (item_elem == NULL) {
    return "";
  }

  return Jid(item_elem->Attr(QN_ATTR_PUBLISHER)).resource();
}
}  // namespace buzz
