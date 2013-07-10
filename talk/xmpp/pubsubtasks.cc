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

#include "talk/xmpp/pubsubtasks.h"

#include <string>
#include <vector>

#include "talk/xmpp/constants.h"
#include "talk/xmpp/receivetask.h"

// An implementation of the tasks for XEP-0060
// (http://xmpp.org/extensions/xep-0060.html).

namespace buzz {

namespace {

bool IsPubSubEventItemsElem(const XmlElement* stanza,
                            const std::string& expected_node) {
  if (stanza->Name() != QN_MESSAGE) {
    return false;
  }

  const XmlElement* event_elem = stanza->FirstNamed(QN_PUBSUB_EVENT);
  if (event_elem == NULL) {
    return false;
  }

  const XmlElement* items_elem = event_elem->FirstNamed(QN_PUBSUB_EVENT_ITEMS);
  if (items_elem == NULL) {
    return false;
  }

  const std::string& actual_node = items_elem->Attr(QN_NODE);
  return (actual_node == expected_node);
}


// Creates <pubsub node="node"><items></pubsub>
XmlElement* CreatePubSubItemsElem(const std::string& node) {
  XmlElement* items_elem = new XmlElement(QN_PUBSUB_ITEMS, false);
  items_elem->AddAttr(QN_NODE, node);
  XmlElement* pubsub_elem = new XmlElement(QN_PUBSUB, false);
  pubsub_elem->AddElement(items_elem);
  return pubsub_elem;
}

// Creates <pubsub node="node"><publish><item id="itemid">payload</item>...
// Takes ownership of payload.
XmlElement* CreatePubSubPublishItemElem(
    const std::string& node,
    const std::string& itemid,
    const std::vector<XmlElement*>& children) {
  XmlElement* pubsub_elem = new XmlElement(QN_PUBSUB, true);
  XmlElement* publish_elem = new XmlElement(QN_PUBSUB_PUBLISH, false);
  publish_elem->AddAttr(QN_NODE, node);
  XmlElement* item_elem = new XmlElement(QN_PUBSUB_ITEM, false);
  item_elem->AddAttr(QN_ID, itemid);
  for (std::vector<XmlElement*>::const_iterator child = children.begin();
       child != children.end(); ++child) {
    item_elem->AddElement(*child);
  }
  publish_elem->AddElement(item_elem);
  pubsub_elem->AddElement(publish_elem);
  return pubsub_elem;
}

// Creates <pubsub node="node"><publish><item id="itemid">payload</item>...
// Takes ownership of payload.
XmlElement* CreatePubSubRetractItemElem(const std::string& node,
                                        const std::string& itemid) {
  XmlElement* pubsub_elem = new XmlElement(QN_PUBSUB, true);
  XmlElement* retract_elem = new XmlElement(QN_PUBSUB_RETRACT, false);
  retract_elem->AddAttr(QN_NODE, node);
  retract_elem->AddAttr(QN_NOTIFY, "true");
  XmlElement* item_elem = new XmlElement(QN_PUBSUB_ITEM, false);
  item_elem->AddAttr(QN_ID, itemid);
  retract_elem->AddElement(item_elem);
  pubsub_elem->AddElement(retract_elem);
  return pubsub_elem;
}

void ParseItem(const XmlElement* item_elem,
               std::vector<PubSubItem>* items) {
  PubSubItem item;
  item.itemid = item_elem->Attr(QN_ID);
  item.elem = item_elem;
  items->push_back(item);
}

// Right now, <retract>s are treated the same as items with empty
// payloads.  We may want to change it in the future, but right now
// it's sufficient for our needs.
void ParseRetract(const XmlElement* retract_elem,
                  std::vector<PubSubItem>* items) {
  ParseItem(retract_elem, items);
}

void ParseEventItemsElem(const XmlElement* stanza,
                         std::vector<PubSubItem>* items) {
  const XmlElement* event_elem = stanza->FirstNamed(QN_PUBSUB_EVENT);
  if (event_elem != NULL) {
    const XmlElement* items_elem =
        event_elem->FirstNamed(QN_PUBSUB_EVENT_ITEMS);
    if (items_elem != NULL) {
      for (const XmlElement* item_elem =
             items_elem->FirstNamed(QN_PUBSUB_EVENT_ITEM);
           item_elem != NULL;
           item_elem = item_elem->NextNamed(QN_PUBSUB_EVENT_ITEM)) {
        ParseItem(item_elem, items);
      }
      for (const XmlElement* retract_elem =
             items_elem->FirstNamed(QN_PUBSUB_EVENT_RETRACT);
           retract_elem != NULL;
           retract_elem = retract_elem->NextNamed(QN_PUBSUB_EVENT_RETRACT)) {
        ParseRetract(retract_elem, items);
      }
    }
  }
}

void ParsePubSubItemsElem(const XmlElement* stanza,
                          std::vector<PubSubItem>* items) {
  const XmlElement* pubsub_elem = stanza->FirstNamed(QN_PUBSUB);
  if (pubsub_elem != NULL) {
    const XmlElement* items_elem = pubsub_elem->FirstNamed(QN_PUBSUB_ITEMS);
    if (items_elem != NULL) {
      for (const XmlElement* item_elem = items_elem->FirstNamed(QN_PUBSUB_ITEM);
           item_elem != NULL;
           item_elem = item_elem->NextNamed(QN_PUBSUB_ITEM)) {
        ParseItem(item_elem, items);
      }
    }
  }
}

}  // namespace

PubSubRequestTask::PubSubRequestTask(XmppTaskParentInterface* parent,
                                     const Jid& pubsubjid,
                                     const std::string& node)
    : IqTask(parent, STR_GET, pubsubjid, CreatePubSubItemsElem(node)) {
}

void PubSubRequestTask::HandleResult(const XmlElement* stanza) {
  std::vector<PubSubItem> items;
  ParsePubSubItemsElem(stanza, &items);
  SignalResult(this, items);
}

bool PubSubReceiveTask::WantsStanza(const XmlElement* stanza) {
  return MatchStanzaFrom(stanza, pubsubjid_) &&
      IsPubSubEventItemsElem(stanza, node_);
}

void PubSubReceiveTask::ReceiveStanza(const XmlElement* stanza) {
  std::vector<PubSubItem> items;
  ParseEventItemsElem(stanza, &items);
  SignalUpdate(this, items);
}

PubSubPublishTask::PubSubPublishTask(XmppTaskParentInterface* parent,
                                     const Jid& pubsubjid,
                                     const std::string& node,
                                     const std::string& itemid,
                                     const std::vector<XmlElement*>& children)
    : IqTask(parent, STR_SET, pubsubjid,
             CreatePubSubPublishItemElem(node, itemid, children)),
      itemid_(itemid) {
}

void PubSubPublishTask::HandleResult(const XmlElement* stanza) {
  SignalResult(this);
}

PubSubRetractTask::PubSubRetractTask(XmppTaskParentInterface* parent,
                                     const Jid& pubsubjid,
                                     const std::string& node,
                                     const std::string& itemid)
    : IqTask(parent, STR_SET, pubsubjid,
             CreatePubSubRetractItemElem(node, itemid)),
      itemid_(itemid) {
}

void PubSubRetractTask::HandleResult(const XmlElement* stanza) {
  SignalResult(this);
}

}  // namespace buzz
