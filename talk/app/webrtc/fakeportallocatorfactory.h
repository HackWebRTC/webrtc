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

// This file defines a fake port allocator factory used for testing.
// This implementation creates instances of cricket::FakePortAllocator.

#ifndef TALK_APP_WEBRTC_FAKEPORTALLOCATORFACTORY_H_
#define TALK_APP_WEBRTC_FAKEPORTALLOCATORFACTORY_H_

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/p2p/client/fakeportallocator.h"

namespace webrtc {

class FakePortAllocatorFactory : public PortAllocatorFactoryInterface {
 public:
  static FakePortAllocatorFactory* Create() {
    talk_base::RefCountedObject<FakePortAllocatorFactory>* allocator =
          new talk_base::RefCountedObject<FakePortAllocatorFactory>();
    return allocator;
  }

  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_configurations,
      const std::vector<TurnConfiguration>& turn_configurations) {
    stun_configs_ = stun_configurations;
    turn_configs_ = turn_configurations;
    return new cricket::FakePortAllocator(talk_base::Thread::Current(), NULL);
  }

  const std::vector<StunConfiguration>& stun_configs() const {
    return stun_configs_;
  }

  const std::vector<TurnConfiguration>& turn_configs() const {
    return turn_configs_;
  }

 protected:
  FakePortAllocatorFactory() {}
  ~FakePortAllocatorFactory() {}

 private:
  std::vector<PortAllocatorFactoryInterface::StunConfiguration> stun_configs_;
  std::vector<PortAllocatorFactoryInterface::TurnConfiguration> turn_configs_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_FAKEPORTALLOCATORFACTORY_H_
