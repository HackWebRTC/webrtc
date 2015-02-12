/*
 * libjingle
 * Copyright 2004--2011 Google Inc.
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

#include "talk/app/webrtc/portallocatorfactory.h"

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/network.h"
#include "webrtc/base/thread.h"

namespace webrtc {

using rtc::scoped_ptr;

rtc::scoped_refptr<PortAllocatorFactoryInterface>
PortAllocatorFactory::Create(
    rtc::Thread* worker_thread) {
  rtc::RefCountedObject<PortAllocatorFactory>* allocator =
        new rtc::RefCountedObject<PortAllocatorFactory>(worker_thread);
  return allocator;
}

PortAllocatorFactory::PortAllocatorFactory(rtc::Thread* worker_thread)
    : network_manager_(new rtc::BasicNetworkManager()),
      socket_factory_(new rtc::BasicPacketSocketFactory(worker_thread)) {
}

PortAllocatorFactory::~PortAllocatorFactory() {}

void PortAllocatorFactory::SetNetworkIgnoreMask(int network_ignore_mask) {
  network_manager_->set_network_ignore_mask(network_ignore_mask);
}

cricket::PortAllocator* PortAllocatorFactory::CreatePortAllocator(
    const std::vector<StunConfiguration>& stun,
    const std::vector<TurnConfiguration>& turn) {
  cricket::ServerAddresses stun_hosts;
  typedef std::vector<StunConfiguration>::const_iterator StunIt;
  for (StunIt stun_it = stun.begin(); stun_it != stun.end(); ++stun_it) {
    stun_hosts.insert(stun_it->server);
  }

  scoped_ptr<cricket::BasicPortAllocator> allocator(
      new cricket::BasicPortAllocator(
          network_manager_.get(), socket_factory_.get(), stun_hosts));

  for (size_t i = 0; i < turn.size(); ++i) {
    cricket::RelayCredentials credentials(turn[i].username, turn[i].password);
    cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
    cricket::ProtocolType protocol;
    if (cricket::StringToProto(turn[i].transport_type.c_str(), &protocol)) {
      relay_server.ports.push_back(cricket::ProtocolAddress(
          turn[i].server, protocol, turn[i].secure));
      relay_server.credentials = credentials;
      // First in the list gets highest priority.
      relay_server.priority = static_cast<int>(turn.size() - i - 1);
      allocator->AddRelay(relay_server);
    } else {
      LOG(LS_WARNING) << "Ignoring TURN server " << turn[i].server << ". "
                      << "Reason= Incorrect " << turn[i].transport_type
                      << " transport parameter.";
    }
  }
  return allocator.release();
}

}  // namespace webrtc
