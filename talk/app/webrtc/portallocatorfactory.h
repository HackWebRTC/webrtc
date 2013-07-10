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

// This file defines the default implementation of
// PortAllocatorFactoryInterface.
// This implementation creates instances of cricket::HTTPPortAllocator and uses
// the BasicNetworkManager and BasicPacketSocketFactory.

#ifndef TALK_APP_WEBRTC_PORTALLOCATORFACTORY_H_
#define TALK_APP_WEBRTC_PORTALLOCATORFACTORY_H_

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/scoped_ptr.h"

namespace cricket {
class PortAllocator;
}

namespace talk_base {
class BasicNetworkManager;
class BasicPacketSocketFactory;
}

namespace webrtc {

class PortAllocatorFactory : public PortAllocatorFactoryInterface {
 public:
  static talk_base::scoped_refptr<PortAllocatorFactoryInterface> Create(
      talk_base::Thread* worker_thread);

  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun,
      const std::vector<TurnConfiguration>& turn);

 protected:
  explicit PortAllocatorFactory(talk_base::Thread* worker_thread);
  ~PortAllocatorFactory();

 private:
  talk_base::scoped_ptr<talk_base::BasicNetworkManager> network_manager_;
  talk_base::scoped_ptr<talk_base::BasicPacketSocketFactory> socket_factory_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PORTALLOCATORFACTORY_H_
