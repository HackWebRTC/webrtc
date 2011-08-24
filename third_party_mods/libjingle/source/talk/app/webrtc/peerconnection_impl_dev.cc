
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

#include "talk/app/webrtc/peerconnection_impl_dev.h"

#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/logging.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/p2p/base/portallocator.h"

namespace webrtc {

PeerConnectionImpl::PeerConnectionImpl(
    cricket::ChannelManager* channel_manager,
    cricket::PortAllocator* port_allocator)
    : observer_(NULL),
      session_(NULL),
      worker_thread_(new talk_base::Thread()),
      channel_manager_(channel_manager),
      port_allocator_(port_allocator) {
  ASSERT(port_allocator_ != NULL);
}

PeerConnectionImpl::~PeerConnectionImpl() {
}

void PeerConnectionImpl::RegisterObserver(PeerConnectionObserver* observer) {
  observer_ = observer;
}

void PeerConnectionImpl::AddStream(LocalMediaStream* local_stream) {
  add_commit_queue_.push_back(local_stream);
}

void PeerConnectionImpl::RemoveStream(LocalMediaStream* remove_stream) {
  remove_commit_queue_.push_back(remove_stream);
}

} // namespace webrtc
