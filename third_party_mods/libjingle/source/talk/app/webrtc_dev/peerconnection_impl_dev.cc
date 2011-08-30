
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

#include "talk/app/webrtc_dev/peerconnection_impl_dev.h"

#include "talk/app/webrtc_dev/scoped_refptr_msg.h"
#include "talk/base/logging.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/p2p/base/portallocator.h"


namespace webrtc {

PeerConnectionImpl::PeerConnectionImpl(
    cricket::ChannelManager* channel_manager,
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* signal_thread)
    : observer_(NULL),
      signal_thread_(signal_thread),
      channel_manager_(channel_manager),
      port_allocator_(port_allocator) {
// TODO(perkj): // ASSERT(port_allocator_ != NULL);
}

PeerConnectionImpl::~PeerConnectionImpl() {
}

void PeerConnectionImpl::RegisterObserver(PeerConnectionObserver* observer) {
  observer_ = observer;
}

void PeerConnectionImpl::AddStream(LocalMediaStream* local_stream) {
  ScopedRefMessageData<LocalMediaStream>* msg =
      new ScopedRefMessageData<LocalMediaStream> (local_stream);
  signal_thread_->Post(this, MSG_ADDMEDIASTREAM, msg);
}

void PeerConnectionImpl::RemoveStream(LocalMediaStream* remove_stream) {
  ScopedRefMessageData<LocalMediaStream>* msg =
        new ScopedRefMessageData<LocalMediaStream> (remove_stream);
  signal_thread_->Post(this, MSG_REMOVEMEDIASTREAM, msg);
}

void PeerConnectionImpl::CommitStreamChanges() {
  signal_thread_->Post(this, MSG_COMMITSTREAMCHANGES);
}

void PeerConnectionImpl::OnMessage(talk_base::Message* msg) {
  talk_base::MessageData* data = msg->pdata;
  switch (msg->message_id) {
    case MSG_ADDMEDIASTREAM: {
      ScopedRefMessageData<LocalMediaStream>* s =
          static_cast<ScopedRefMessageData<LocalMediaStream>*> (data);
      LocalStreamMap::iterator it =
          local_media_streams_.find(s->data()->label());
      if (it != local_media_streams_.end())
        return;  // Stream already exist.
      const std::string& label = s->data()->label();
      local_media_streams_[label] = s->data();
      break;
    }
    case MSG_REMOVEMEDIASTREAM: {
      ScopedRefMessageData<LocalMediaStream>* s =
          static_cast<ScopedRefMessageData<LocalMediaStream>*> (data);
      local_media_streams_.erase(s->data()->label());
      break;
    }
    case MSG_COMMITSTREAMCHANGES: {
      // TODO(perkj): Here is where necessary signaling
      // and creation of channels should happen. Also removing of channels.
      // The media streams are in the local_media_streams_ array.
      break;
    }
  }
}

}  // namespace webrtc
