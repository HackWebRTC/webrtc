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

#include "talk/app/webrtc_dev/mediastreamproxy.h"

namespace {

enum {
  MSG_REGISTER_OBSERVER = 1,
  MSG_UNREGISTER_OBSERVER,
  MSG_ADD_TRACK,
  MSG_READY_STATE,
  MSG_SET_READY_STATE,
  MSG_COUNT,
  MSG_AT
};

typedef talk_base::TypedMessageData<size_t> SizeTMessageData;
typedef talk_base::TypedMessageData<webrtc::Observer*> ObserverMessageData;
typedef talk_base::TypedMessageData<webrtc::MediaStream::ReadyState>
    ReadyStateMessageData;


class MediaStreamTrackMessageData : public talk_base::MessageData {
 public:
  explicit MediaStreamTrackMessageData(webrtc::MediaStreamTrack* track)
      : track_(track),
        result_(false) {
  }

  scoped_refptr<webrtc::MediaStreamTrack> track_;
  bool result_;
};

class MediaStreamTrackAtMessageData : public talk_base::MessageData {
 public:
  explicit MediaStreamTrackAtMessageData(size_t index)
      : index_(index) {
  }

  size_t index_;
  scoped_refptr<webrtc::MediaStreamTrack> track_;
};

}  // namespace anonymous

namespace webrtc {

scoped_refptr<MediaStreamProxy> MediaStreamProxy::Create(
    const std::string& label,
    talk_base::Thread* signaling_thread) {
  ASSERT(signaling_thread);
  RefCountImpl<MediaStreamProxy>* stream =
      new RefCountImpl<MediaStreamProxy>(label, signaling_thread);
  return stream;
}

MediaStreamProxy::MediaStreamProxy(const std::string& label,
                                   talk_base::Thread* signaling_thread)
    : signaling_thread_(signaling_thread),
      media_stream_impl_(MediaStreamImpl::Create(label)),
      track_list_(new RefCountImpl<MediaStreamTrackListProxy>(
          media_stream_impl_->tracks(),
          signaling_thread_)) {
}

const std::string& MediaStreamProxy::label() {
  return media_stream_impl_->label();
}

scoped_refptr<MediaStreamTrackList> MediaStreamProxy::tracks() {
  return track_list_;
}

MediaStream::ReadyState MediaStreamProxy::ready_state() {
  if (!signaling_thread_->IsCurrent()) {
    ReadyStateMessageData msg(MediaStream::kInitializing);
    Send(MSG_READY_STATE, &msg);
    return msg.data();
  }
  return media_stream_impl_->ready_state();
}

void MediaStreamProxy::set_ready_state(MediaStream::ReadyState new_state) {
  if (!signaling_thread_->IsCurrent()) {
    ReadyStateMessageData msg(MediaStream::kInitializing);
    Send(MSG_SET_READY_STATE, &msg);
    return;
  }
  media_stream_impl_->set_ready_state(new_state);
}

bool MediaStreamProxy::AddTrack(MediaStreamTrack* track) {
  if (!signaling_thread_->IsCurrent()) {
    MediaStreamTrackMessageData msg(track);
    Send(MSG_ADD_TRACK, &msg);
    return msg.result_;
  }
  return media_stream_impl_->AddTrack(track);
}

void MediaStreamProxy::RegisterObserver(Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_REGISTER_OBSERVER, &msg);
    return;
  }
  media_stream_impl_->RegisterObserver(observer);
}

void MediaStreamProxy::UnregisterObserver(Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_UNREGISTER_OBSERVER, &msg);
    return;
  }
  media_stream_impl_->UnregisterObserver(observer);
}

void MediaStreamProxy::Send(uint32 id, talk_base::MessageData* data) {
  signaling_thread_->Send(this, id, data);
}

// Implement MessageHandler
void MediaStreamProxy::OnMessage(talk_base::Message* msg) {
  talk_base::MessageData* data = msg->pdata;
  switch (msg->message_id) {
    case MSG_REGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      media_stream_impl_->RegisterObserver(observer->data());
      break;
    }
    case MSG_UNREGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      media_stream_impl_->UnregisterObserver(observer->data());
      break;
    }
    case MSG_ADD_TRACK: {
      MediaStreamTrackMessageData * track =
          static_cast<MediaStreamTrackMessageData*>(data);
      track->result_ = media_stream_impl_->AddTrack(track->track_.get());
      break;
    }
    case MSG_READY_STATE: {
      ReadyStateMessageData* state = static_cast<ReadyStateMessageData*>(data);
      state->data() = media_stream_impl_->ready_state();
      break;
    }
    case MSG_SET_READY_STATE: {
      ReadyStateMessageData* state = static_cast<ReadyStateMessageData*>(data);
      media_stream_impl_->set_ready_state(state->data());
      break;
    }
    default:
      ASSERT(!"Not Implemented!");
      break;
  }
}

MediaStreamProxy::MediaStreamTrackListProxy::MediaStreamTrackListProxy(
    MediaStreamTrackList* track_list,
    talk_base::Thread* signaling_thread)
    :  track_list_(track_list),
       signaling_thread_(signaling_thread) {
}

size_t MediaStreamProxy::MediaStreamTrackListProxy::count() {
  if (!signaling_thread_->IsCurrent()) {
    SizeTMessageData msg(0u);
    Send(MSG_COUNT, &msg);
    return msg.data();
  }
  return track_list_->count();
}

scoped_refptr<MediaStreamTrack> MediaStreamProxy::MediaStreamTrackListProxy::at(
    size_t index) {
  if (!signaling_thread_->IsCurrent()) {
    MediaStreamTrackAtMessageData msg(index);
    Send(MSG_AT, &msg);
    return msg.track_;
  }
  return track_list_->at(index);
}

void MediaStreamProxy::MediaStreamTrackListProxy::RegisterObserver(
    Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_REGISTER_OBSERVER, &msg);
    return;
  }
  track_list_->RegisterObserver(observer);
}

void MediaStreamProxy::MediaStreamTrackListProxy::UnregisterObserver(
    Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_UNREGISTER_OBSERVER, &msg);
    return;
  }
  track_list_->UnregisterObserver(observer);
}

void MediaStreamProxy::MediaStreamTrackListProxy::Send(
    uint32 id, talk_base::MessageData* data) {
  signaling_thread_->Send(this, id, data);
}

// Implement MessageHandler
void MediaStreamProxy::MediaStreamTrackListProxy::OnMessage(
    talk_base::Message* msg) {
  talk_base::MessageData* data = msg->pdata;
  switch (msg->message_id) {
    case MSG_REGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      track_list_->RegisterObserver(observer->data());
      break;
    }
    case MSG_UNREGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      track_list_->UnregisterObserver(observer->data());
      break;
    }
    case MSG_COUNT: {
      SizeTMessageData* count = static_cast<SizeTMessageData*>(data);
      count->data() = track_list_->count();
      break;
    }
    case MSG_AT: {
      MediaStreamTrackAtMessageData* track =
          static_cast<MediaStreamTrackAtMessageData*>(data);
      track->track_ = track_list_->at(track->index_);
      break;
    }
    default:
      ASSERT(!"Not Implemented!");
      break;
  }
}

}  // namespace webrtc
