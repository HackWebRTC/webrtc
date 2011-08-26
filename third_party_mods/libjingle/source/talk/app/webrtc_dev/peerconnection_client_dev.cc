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

#include "talk/app/webrtc/peerconnectionmanager.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"

int main() {
  LOG(INFO) << "Create PeerConnectionManager.";

  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<talk_base::Thread> worker_thread_;

  port_allocator_.reset(new cricket::BasicPortAllocator(
     new talk_base::BasicNetworkManager(),
     talk_base::SocketAddress("stun.l.google.com", 19302),
     talk_base::SocketAddress(),
     talk_base::SocketAddress(), talk_base::SocketAddress()));

  worker_thread_.reset(new talk_base::Thread());
  if (!worker_thread_->SetName("workder thread", NULL) ||
     !worker_thread_->Start()) {
   LOG(WARNING) << "Failed to start libjingle workder thread";
  }

  webrtc::PeerConnectionManager* peerconnection_manager =
      webrtc::PeerConnectionManager::Create(port_allocator_.get(),
                                            worker_thread_.get());
  return 0;
}
