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

#ifndef TALK_XMPP_PUBSUBSTATECLIENT_H_
#define TALK_XMPP_PUBSUBSTATECLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/pubsubclient.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"

namespace buzz {

// To handle retracts correctly, we need to remember certain details
// about an item.  We could just cache the entire XML element, but
// that would take more memory and require re-parsing.
struct StateItemInfo {
  std::string published_nick;
  std::string publisher_nick;
};

// Represents a PubSub state change.  Usually, the key is the nick,
// but not always.  It's a per-state-type thing.  Look below on how keys are
// computed.
template <typename C>
struct PubSubStateChange {
  // The nick of the user changing the state.
  std::string publisher_nick;
  // The nick of the user whose state is changing.
  std::string published_nick;
  C old_state;
  C new_state;
};

// Knows how to handle specific states and XML.
template <typename C>
class PubSubStateSerializer {
 public:
  virtual ~PubSubStateSerializer() {}
  virtual XmlElement* Write(const QName& state_name, const C& state) = 0;
  virtual void Parse(const XmlElement* state_elem, C* state_out) = 0;
};

// Knows how to create "keys" for states, which determines their
// uniqueness.  Most states are per-nick, but block is
// per-blocker-and-blockee.  This is independent of itemid, especially
// in the case of presenter state.
class PubSubStateKeySerializer {
 public:
  virtual ~PubSubStateKeySerializer() {}
  virtual std::string GetKey(const std::string& publisher_nick,
                             const std::string& published_nick) = 0;
};

class PublishedNickKeySerializer : public PubSubStateKeySerializer {
 public:
  virtual std::string GetKey(const std::string& publisher_nick,
                             const std::string& published_nick);
};

class PublisherAndPublishedNicksKeySerializer
  : public PubSubStateKeySerializer {
 public:
  virtual std::string GetKey(const std::string& publisher_nick,
                             const std::string& published_nick);
};

// Adapts PubSubClient to be specifically suited for pub sub call
// states.  Signals state changes and keeps track of keys, which are
// normally nicks.
template <typename C>
class PubSubStateClient : public sigslot::has_slots<> {
 public:
  // Gets ownership of the serializers, but not the client.
  PubSubStateClient(const std::string& publisher_nick,
                    PubSubClient* client,
                    const QName& state_name,
                    C default_state,
                    PubSubStateKeySerializer* key_serializer,
                    PubSubStateSerializer<C>* state_serializer)
      : publisher_nick_(publisher_nick),
        client_(client),
        state_name_(state_name),
        default_state_(default_state) {
    key_serializer_.reset(key_serializer);
    state_serializer_.reset(state_serializer);
    client_->SignalItems.connect(
        this, &PubSubStateClient<C>::OnItems);
    client_->SignalPublishResult.connect(
        this, &PubSubStateClient<C>::OnPublishResult);
    client_->SignalPublishError.connect(
        this, &PubSubStateClient<C>::OnPublishError);
    client_->SignalRetractResult.connect(
        this, &PubSubStateClient<C>::OnRetractResult);
    client_->SignalRetractError.connect(
        this, &PubSubStateClient<C>::OnRetractError);
  }

  virtual ~PubSubStateClient() {}

  virtual void Publish(const std::string& published_nick,
                       const C& state,
                       std::string* task_id_out) {
    std::string key = key_serializer_->GetKey(publisher_nick_, published_nick);
    std::string itemid = state_name_.LocalPart() + ":" + key;
    if (StatesEqual(state, default_state_)) {
      client_->RetractItem(itemid, task_id_out);
    } else {
      XmlElement* state_elem = state_serializer_->Write(state_name_, state);
      state_elem->AddAttr(QN_NICK, published_nick);
      client_->PublishItem(itemid, state_elem, task_id_out);
    }
  }

  sigslot::signal1<const PubSubStateChange<C>&> SignalStateChange;
  // Signal (task_id, item).  item is NULL for retract.
  sigslot::signal2<const std::string&,
  const XmlElement*> SignalPublishResult;
  // Signal (task_id, item, error stanza).  item is NULL for retract.
  sigslot::signal3<const std::string&,
  const XmlElement*,
  const XmlElement*> SignalPublishError;

 protected:
  // return false if retracted item (no info or state given)
  virtual bool ParseStateItem(const PubSubItem& item,
                              StateItemInfo* info_out,
                              C* state_out) {
    const XmlElement* state_elem = item.elem->FirstNamed(state_name_);
    if (state_elem == NULL) {
      return false;
    }

    info_out->publisher_nick =
        client_->GetPublisherNickFromPubSubItem(item.elem);
    info_out->published_nick = state_elem->Attr(QN_NICK);
    state_serializer_->Parse(state_elem, state_out);
    return true;
  }

  virtual bool StatesEqual(const C& state1, const C& state2) {
    return state1 == state2;
  }

  PubSubClient* client() { return client_; }
  const QName& state_name() { return state_name_; }

 private:
  void OnItems(PubSubClient* pub_sub_client,
               const std::vector<PubSubItem>& items) {
    for (std::vector<PubSubItem>::const_iterator item = items.begin();
         item != items.end(); ++item) {
      OnItem(*item);
    }
  }

  void OnItem(const PubSubItem& item) {
    const std::string& itemid = item.itemid;
    StateItemInfo info;
    C new_state;

    bool retracted = !ParseStateItem(item, &info, &new_state);
    if (retracted) {
      bool known_itemid =
          (info_by_itemid_.find(itemid) != info_by_itemid_.end());
      if (!known_itemid) {
        // Nothing to retract, and nothing to publish.
        // Probably a different state type.
        return;
      } else {
        info = info_by_itemid_[itemid];
        info_by_itemid_.erase(itemid);
        new_state = default_state_;
      }
    } else {
      // TODO: Assert new key matches the known key. It
      // shouldn't change!
      info_by_itemid_[itemid] = info;
    }

    std::string key = key_serializer_->GetKey(
        info.publisher_nick, info.published_nick);
    bool has_old_state = (state_by_key_.find(key) != state_by_key_.end());
    C old_state = has_old_state ? state_by_key_[key] : default_state_;
    if ((retracted && !has_old_state) || StatesEqual(new_state, old_state)) {
      // Nothing change, so don't bother signalling.
      return;
    }

    if (retracted || StatesEqual(new_state, default_state_)) {
      // We treat a default state similar to a retract.
      state_by_key_.erase(key);
    } else {
      state_by_key_[key] = new_state;
    }

    PubSubStateChange<C> change;
    if (!retracted) {
      // Retracts do not have publisher information.
      change.publisher_nick = info.publisher_nick;
    }
    change.published_nick = info.published_nick;
    change.old_state = old_state;
    change.new_state = new_state;
    SignalStateChange(change);
  }

  void OnPublishResult(PubSubClient* pub_sub_client,
                       const std::string& task_id,
                       const XmlElement* item) {
    SignalPublishResult(task_id, item);
  }

  void OnPublishError(PubSubClient* pub_sub_client,
                      const std::string& task_id,
                      const buzz::XmlElement* item,
                      const buzz::XmlElement* stanza) {
    SignalPublishError(task_id, item, stanza);
  }

  void OnRetractResult(PubSubClient* pub_sub_client,
                       const std::string& task_id) {
    // There's no point in differentiating between publish and retract
    // errors, so we simplify by making them both signal a publish
    // result.
    const XmlElement* item = NULL;
    SignalPublishResult(task_id, item);
  }

  void OnRetractError(PubSubClient* pub_sub_client,
                      const std::string& task_id,
                      const buzz::XmlElement* stanza) {
    // There's no point in differentiating between publish and retract
    // errors, so we simplify by making them both signal a publish
    // error.
    const XmlElement* item = NULL;
    SignalPublishError(task_id, item, stanza);
  }

  std::string publisher_nick_;
  PubSubClient* client_;
  const QName state_name_;
  C default_state_;
  talk_base::scoped_ptr<PubSubStateKeySerializer> key_serializer_;
  talk_base::scoped_ptr<PubSubStateSerializer<C> > state_serializer_;
  // key => state
  std::map<std::string, C> state_by_key_;
  // itemid => StateItemInfo
  std::map<std::string, StateItemInfo> info_by_itemid_;

  DISALLOW_COPY_AND_ASSIGN(PubSubStateClient);
};
}  // namespace buzz

#endif  // TALK_XMPP_PUBSUBSTATECLIENT_H_
