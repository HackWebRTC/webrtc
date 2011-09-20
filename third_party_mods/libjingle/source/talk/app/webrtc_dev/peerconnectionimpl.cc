
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

#include "talk/app/webrtc_dev/peerconnectionimpl.h"

#include <vector>

#include "talk/app/webrtc_dev/scoped_refptr_msg.h"
#include "talk/app/webrtc_dev/streamcollectionimpl.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/channelmanager.h"

namespace webrtc {

PeerConnectionImpl::PeerConnectionImpl(
    cricket::ChannelManager* channel_manager,
    talk_base::Thread* worker_thread,
    PcNetworkManager* network_manager,
    PcPacketSocketFactory* socket_factory)
    : observer_(NULL),
      local_media_streams_(StreamCollectionImpl::Create()),
      worker_thread_(worker_thread),
      channel_manager_(channel_manager),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      port_allocator_(new cricket::BasicPortAllocator(
          network_manager->network_manager(),
          socket_factory->socket_factory())) {
}

PeerConnectionImpl::~PeerConnectionImpl() {
  worker_thread_->Clear(this);
}

bool PeerConnectionImpl::Initialize(const std::string& configuration) {
  // TODO(perkj): More initialization code?
  return true;
}

void PeerConnectionImpl::RegisterObserver(PeerConnectionObserver* observer) {
  observer_ = observer;
}

scoped_refptr<StreamCollection> PeerConnectionImpl::local_streams() {
  return local_media_streams_;
}

void PeerConnectionImpl::AddStream(LocalMediaStream* local_stream) {
  local_media_streams_->AddStream(local_stream);
}

void PeerConnectionImpl::RemoveStream(LocalMediaStream* remove_stream) {
  local_media_streams_->RemoveStream(remove_stream);
}

void PeerConnectionImpl::CommitStreamChanges() {
  ScopedRefMessageData<StreamCollectionImpl>* msg =
           new ScopedRefMessageData<StreamCollectionImpl> (
               StreamCollectionImpl::Create(local_media_streams_));
  worker_thread_->Post(this, MSG_COMMITSTREAMCHANGES, msg);
}

void PeerConnectionImpl::OnMessage(talk_base::Message* msg) {
  talk_base::MessageData* data = msg->pdata;
  switch (msg->message_id) {
    case MSG_COMMITSTREAMCHANGES: {
      // TODO(perkj): Here is where necessary signaling
      // and creation of channels should happen. Also removing of channels.
      // The media streams are in the LocalStreamCollection in data.
      // The collection is a copy of the local_media_streams_ and only
      // accessible in this thread context.
      break;
    }
  }
  delete data;  // because it is Posted
}

}  // namespace webrtc
