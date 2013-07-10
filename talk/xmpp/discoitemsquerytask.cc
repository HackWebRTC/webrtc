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

#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/discoitemsquerytask.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

DiscoItemsQueryTask::DiscoItemsQueryTask(XmppTaskParentInterface* parent,
                                         const Jid& to,
                                         const std::string& node)
    : IqTask(parent, STR_GET, to, MakeRequest(node)) {
}

XmlElement* DiscoItemsQueryTask::MakeRequest(const std::string& node) {
  XmlElement* element = new XmlElement(QN_DISCO_ITEMS_QUERY, true);
  if (!node.empty()) {
    element->AddAttr(QN_NODE, node);
  }
  return element;
}

void DiscoItemsQueryTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query = stanza->FirstNamed(QN_DISCO_ITEMS_QUERY);
  if (query) {
    std::vector<DiscoItem> items;
    for (const buzz::XmlChild* child = query->FirstChild(); child;
         child = child->NextChild()) {
      DiscoItem item;
      const buzz::XmlElement* child_element = child->AsElement();
      if (ParseItem(child_element, &item)) {
        items.push_back(item);
      }
    }
    SignalResult(items);
  } else {
    SignalError(this, NULL);
  }
}

bool DiscoItemsQueryTask::ParseItem(const XmlElement* element,
                                    DiscoItem* item) {
  if (element->HasAttr(QN_JID)) {
    return false;
  }

  item->jid = element->Attr(QN_JID);
  item->name = element->Attr(QN_NAME);
  item->node = element->Attr(QN_NODE);
  return true;
}

}  // namespace buzz
