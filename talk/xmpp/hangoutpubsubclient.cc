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

#include "talk/xmpp/hangoutpubsubclient.h"

#include "talk/base/logging.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"


// Gives a high-level API for MUC call PubSub needs such as
// presenter state, recording state, mute state, and remote mute.

namespace buzz {

namespace {
const char kPresenting[] = "s";
const char kNotPresenting[] = "o";
const char kEmpty[] = "";

const std::string GetPublisherNickFromPubSubItem(const XmlElement* item_elem) {
  if (item_elem == NULL) {
    return "";
  }

  return Jid(item_elem->Attr(QN_ATTR_PUBLISHER)).resource();
}

}  // namespace


// Knows how to handle specific states and XML.
template <typename C>
class PubSubStateSerializer {
 public:
  virtual ~PubSubStateSerializer() {}
  virtual XmlElement* Write(const QName& state_name, const C& state) = 0;
  virtual C Parse(const XmlElement* state_elem) = 0;
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
                             const std::string& published_nick) {
    return published_nick;
  }
};

class PublisherAndPublishedNicksKeySerializer
    : public PubSubStateKeySerializer {
 public:
  virtual std::string GetKey(const std::string& publisher_nick,
                             const std::string& published_nick) {
    return publisher_nick + ":" + published_nick;
  }
};

// A simple serialiazer where presence of item => true, lack of item
// => false.
class BoolStateSerializer : public PubSubStateSerializer<bool> {
  virtual XmlElement* Write(const QName& state_name, const bool& state) {
    if (!state) {
      return NULL;
    }

    return new XmlElement(state_name, true);
  }

  virtual bool Parse(const XmlElement* state_elem) {
    return state_elem != NULL;
  }
};

// Adapts PubSubClient to be specifically suited for pub sub call
// states.  Signals state changes and keeps track of keys, which are
// normally nicks.
// TODO: Expose this as a generally useful class, not just
// private to hangouts.
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
  };

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
                              bool* state_out) {
    const XmlElement* state_elem = item.elem->FirstNamed(state_name_);
    if (state_elem == NULL) {
      return false;
    }

    info_out->publisher_nick = GetPublisherNickFromPubSubItem(item.elem);
    info_out->published_nick = state_elem->Attr(QN_NICK);
    *state_out = state_serializer_->Parse(state_elem);
    return true;
  };

  virtual bool StatesEqual(C state1, C state2) {
    return state1 == state2;
  }

  PubSubClient* client() { return client_; }

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
    change.publisher_nick = info.publisher_nick;
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
};

class PresenterStateClient : public PubSubStateClient<bool> {
 public:
  PresenterStateClient(const std::string& publisher_nick,
                       PubSubClient* client,
                       const QName& state_name,
                       bool default_state)
      : PubSubStateClient<bool>(
          publisher_nick, client, state_name, default_state,
          new PublishedNickKeySerializer(), NULL) {
  }

  virtual void Publish(const std::string& published_nick,
                       const bool& state,
                       std::string* task_id_out) {
    XmlElement* presenter_elem = new XmlElement(QN_PRESENTER_PRESENTER, true);
    presenter_elem->AddAttr(QN_NICK, published_nick);

    XmlElement* presentation_item_elem =
        new XmlElement(QN_PRESENTER_PRESENTATION_ITEM, false);
    const std::string& presentation_type = state ? kPresenting : kNotPresenting;
    presentation_item_elem->AddAttr(
        QN_PRESENTER_PRESENTATION_TYPE, presentation_type);

    // The Presenter state is kind of dumb in that it doesn't always use
    // retracts.  It relies on setting the "type" to a special value.
    std::string itemid = published_nick;
    std::vector<XmlElement*> children;
    children.push_back(presenter_elem);
    children.push_back(presentation_item_elem);
    client()->PublishItem(itemid, children, task_id_out);
  }

 protected:
  virtual bool ParseStateItem(const PubSubItem& item,
                              StateItemInfo* info_out,
                              bool* state_out) {
    const XmlElement* presenter_elem =
        item.elem->FirstNamed(QN_PRESENTER_PRESENTER);
    const XmlElement* presentation_item_elem =
        item.elem->FirstNamed(QN_PRESENTER_PRESENTATION_ITEM);
    if (presentation_item_elem == NULL || presenter_elem == NULL) {
      return false;
    }

    info_out->publisher_nick = GetPublisherNickFromPubSubItem(item.elem);
    info_out->published_nick = presenter_elem->Attr(QN_NICK);
    *state_out = (presentation_item_elem->Attr(
        QN_PRESENTER_PRESENTATION_TYPE) != kNotPresenting);
    return true;
  }

  virtual bool StatesEqual(bool state1, bool state2) {
    return false;  // Make every item trigger an event, even if state doesn't change.
  }
};

HangoutPubSubClient::HangoutPubSubClient(XmppTaskParentInterface* parent,
                                         const Jid& mucjid,
                                         const std::string& nick)
    : mucjid_(mucjid),
      nick_(nick) {
  presenter_client_.reset(new PubSubClient(parent, mucjid, NS_PRESENTER));
  presenter_client_->SignalRequestError.connect(
      this, &HangoutPubSubClient::OnPresenterRequestError);

  media_client_.reset(new PubSubClient(parent, mucjid, NS_GOOGLE_MUC_MEDIA));
  media_client_->SignalRequestError.connect(
      this, &HangoutPubSubClient::OnMediaRequestError);

  presenter_state_client_.reset(new PresenterStateClient(
      nick_, presenter_client_.get(), QN_PRESENTER_PRESENTER, false));
  presenter_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnPresenterStateChange);
  presenter_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnPresenterPublishResult);
  presenter_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnPresenterPublishError);

  audio_mute_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_AUDIO_MUTE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  // Can't just repeat because we need to watch for remote mutes.
  audio_mute_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnAudioMuteStateChange);
  audio_mute_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnAudioMutePublishResult);
  audio_mute_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnAudioMutePublishError);

  video_mute_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_VIDEO_MUTE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  // Can't just repeat because we need to watch for remote mutes.
  video_mute_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnVideoMuteStateChange);
  video_mute_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnVideoMutePublishResult);
  video_mute_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnVideoMutePublishError);

  video_pause_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_VIDEO_PAUSE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  video_pause_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnVideoPauseStateChange);
  video_pause_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnVideoPausePublishResult);
  video_pause_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnVideoPausePublishError);

  recording_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_RECORDING, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  recording_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnRecordingStateChange);
  recording_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnRecordingPublishResult);
  recording_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnRecordingPublishError);

  media_block_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_MEDIA_BLOCK, false,
      new PublisherAndPublishedNicksKeySerializer(),
      new BoolStateSerializer()));
  media_block_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnMediaBlockStateChange);
  media_block_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnMediaBlockPublishResult);
  media_block_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnMediaBlockPublishError);
}

HangoutPubSubClient::~HangoutPubSubClient() {
}

void HangoutPubSubClient::RequestAll() {
  presenter_client_->RequestItems();
  media_client_->RequestItems();
}

void HangoutPubSubClient::OnPresenterRequestError(
    PubSubClient* client, const XmlElement* stanza) {
  SignalRequestError(client->node(), stanza);
}

void HangoutPubSubClient::OnMediaRequestError(
    PubSubClient* client, const XmlElement* stanza) {
  SignalRequestError(client->node(), stanza);
}

void HangoutPubSubClient::PublishPresenterState(
    bool presenting, std::string* task_id_out) {
  presenter_state_client_->Publish(nick_, presenting, task_id_out);
}

void HangoutPubSubClient::PublishAudioMuteState(
    bool muted, std::string* task_id_out) {
  audio_mute_state_client_->Publish(nick_, muted, task_id_out);
}

void HangoutPubSubClient::PublishVideoMuteState(
    bool muted, std::string* task_id_out) {
  video_mute_state_client_->Publish(nick_, muted, task_id_out);
}

void HangoutPubSubClient::PublishVideoPauseState(
    bool paused, std::string* task_id_out) {
  video_pause_state_client_->Publish(nick_, paused, task_id_out);
}

void HangoutPubSubClient::PublishRecordingState(
    bool recording, std::string* task_id_out) {
  recording_state_client_->Publish(nick_, recording, task_id_out);
}

// Remote mute is accomplished by setting another client's mute state.
void HangoutPubSubClient::RemoteMute(
    const std::string& mutee_nick, std::string* task_id_out) {
  audio_mute_state_client_->Publish(mutee_nick, true, task_id_out);
}

// Block media is accomplished by setting another client's block
// state, kind of like remote mute.
void HangoutPubSubClient::BlockMedia(
    const std::string& blockee_nick, std::string* task_id_out) {
  media_block_state_client_->Publish(blockee_nick, true, task_id_out);
}

void HangoutPubSubClient::OnPresenterStateChange(
    const PubSubStateChange<bool>& change) {
  SignalPresenterStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnPresenterPublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishPresenterResult(task_id);
}

void HangoutPubSubClient::OnPresenterPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishPresenterError(task_id, stanza);
}

// Since a remote mute is accomplished by another client setting our
// mute state, if our state changes to muted, we should mute ourselves.
// Note that remote un-muting is disallowed by the RoomServer.
void HangoutPubSubClient::OnAudioMuteStateChange(
    const PubSubStateChange<bool>& change) {
  bool was_muted = change.old_state;
  bool is_muted = change.new_state;
  bool remote_action = (!change.publisher_nick.empty() &&
                        (change.publisher_nick != change.published_nick));
  if (remote_action) {
    const std::string& mutee_nick = change.published_nick;
    const std::string& muter_nick = change.publisher_nick;
    if (!is_muted) {
      // The server should prevent remote un-mute.
      LOG(LS_WARNING) << muter_nick << " remote unmuted " << mutee_nick;
      return;
    }
    bool should_mute_locally = (mutee_nick == nick_);
    SignalRemoteMute(mutee_nick, muter_nick, should_mute_locally);
  } else {
    SignalAudioMuteStateChange(change.published_nick, was_muted, is_muted);
  }
}

const std::string GetAudioMuteNickFromItem(const XmlElement* item) {
  if (item != NULL) {
    const XmlElement* audio_mute_state =
        item->FirstNamed(QN_GOOGLE_MUC_AUDIO_MUTE);
    if (audio_mute_state != NULL) {
      return audio_mute_state->Attr(QN_NICK);
    }
  }
  return std::string();
}

const std::string GetBlockeeNickFromItem(const XmlElement* item) {
  if (item != NULL) {
    const XmlElement* media_block_state =
        item->FirstNamed(QN_GOOGLE_MUC_MEDIA_BLOCK);
    if (media_block_state != NULL) {
      return media_block_state->Attr(QN_NICK);
    }
  }
  return std::string();
}

void HangoutPubSubClient::OnAudioMutePublishResult(
    const std::string& task_id, const XmlElement* item) {
  const std::string& mutee_nick = GetAudioMuteNickFromItem(item);
  if (mutee_nick != nick_) {
    SignalRemoteMuteResult(task_id, mutee_nick);
  } else {
    SignalPublishAudioMuteResult(task_id);
  }
}

void HangoutPubSubClient::OnAudioMutePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  const std::string& mutee_nick = GetAudioMuteNickFromItem(item);
  if (mutee_nick != nick_) {
    SignalRemoteMuteError(task_id, mutee_nick, stanza);
  } else {
    SignalPublishAudioMuteError(task_id, stanza);
  }
}

void HangoutPubSubClient::OnVideoMuteStateChange(
    const PubSubStateChange<bool>& change) {
  SignalVideoMuteStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnVideoMutePublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishVideoMuteResult(task_id);
}

void HangoutPubSubClient::OnVideoMutePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishVideoMuteError(task_id, stanza);
}

void HangoutPubSubClient::OnVideoPauseStateChange(
    const PubSubStateChange<bool>& change) {
  SignalVideoPauseStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnVideoPausePublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishVideoPauseResult(task_id);
}

void HangoutPubSubClient::OnVideoPausePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishVideoPauseError(task_id, stanza);
}

void HangoutPubSubClient::OnRecordingStateChange(
    const PubSubStateChange<bool>& change) {
  SignalRecordingStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnRecordingPublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishRecordingResult(task_id);
}

void HangoutPubSubClient::OnRecordingPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishRecordingError(task_id, stanza);
}

void HangoutPubSubClient::OnMediaBlockStateChange(
    const PubSubStateChange<bool>& change) {
  const std::string& blockee_nick = change.published_nick;
  const std::string& blocker_nick = change.publisher_nick;

  bool was_blockee = change.old_state;
  bool is_blockee = change.new_state;
  if (!was_blockee && is_blockee) {
    SignalMediaBlock(blockee_nick, blocker_nick);
  }
  // TODO: Should we bother signaling unblock? Currently
  // it isn't allowed, but it might happen when a participant leaves
  // the room and the item is retracted.
}

void HangoutPubSubClient::OnMediaBlockPublishResult(
    const std::string& task_id, const XmlElement* item) {
  const std::string& blockee_nick = GetBlockeeNickFromItem(item);
  SignalMediaBlockResult(task_id, blockee_nick);
}

void HangoutPubSubClient::OnMediaBlockPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  const std::string& blockee_nick = GetBlockeeNickFromItem(item);
  SignalMediaBlockError(task_id, blockee_nick, stanza);
}

}  // namespace buzz
