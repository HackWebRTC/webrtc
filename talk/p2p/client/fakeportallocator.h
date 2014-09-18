/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#ifndef TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_
#define TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_

#include <string>
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/udpport.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {
class SocketFactory;
class Thread;
}

namespace cricket {

class FakePortAllocatorSession : public PortAllocatorSession {
 public:
  FakePortAllocatorSession(rtc::Thread* worker_thread,
                           rtc::PacketSocketFactory* factory,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd)
      : PortAllocatorSession(content_name, component, ice_ufrag, ice_pwd,
                             cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG),
        worker_thread_(worker_thread),
        factory_(factory),
        network_("network", "unittest",
                 rtc::IPAddress(INADDR_LOOPBACK), 8),
        port_(), running_(false),
        port_config_count_(0) {
    network_.AddIP(rtc::IPAddress(INADDR_LOOPBACK));
  }

  virtual void StartGettingPorts() {
    if (!port_) {
      port_.reset(cricket::UDPPort::Create(worker_thread_,
                                           factory_,
                                           &network_,
#ifdef USE_WEBRTC_DEV_BRANCH
                                           network_.GetBestIP(),
#else  // USE_WEBRTC_DEV_BRANCH
                                           network_.ip(),
#endif  // USE_WEBRTC_DEV_BRANCH
                                           0,
                                           0,
                                           username(),
                                           password()));
      AddPort(port_.get());
    }
    ++port_config_count_;
    running_ = true;
  }

  virtual void StopGettingPorts() { running_ = false; }
  virtual bool IsGettingPorts() { return running_; }
  int port_config_count() { return port_config_count_; }

  void AddPort(cricket::Port* port) {
    port->set_component(component_);
    port->set_generation(0);
    port->SignalPortComplete.connect(
        this, &FakePortAllocatorSession::OnPortComplete);
    port->PrepareAddress();
    SignalPortReady(this, port);
  }
  void OnPortComplete(cricket::Port* port) {
    SignalCandidatesReady(this, port->Candidates());
    SignalCandidatesAllocationDone(this);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::Network network_;
  rtc::scoped_ptr<cricket::Port> port_;
  bool running_;
  int port_config_count_;
};

class FakePortAllocator : public cricket::PortAllocator {
 public:
  FakePortAllocator(rtc::Thread* worker_thread,
                    rtc::PacketSocketFactory* factory)
      : worker_thread_(worker_thread), factory_(factory) {
    if (factory_ == NULL) {
      owned_factory_.reset(new rtc::BasicPacketSocketFactory(
          worker_thread_));
      factory_ = owned_factory_.get();
    }
  }

  virtual cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) {
    return new FakePortAllocatorSession(
        worker_thread_, factory_, content_name, component, ice_ufrag, ice_pwd);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::scoped_ptr<rtc::BasicPacketSocketFactory> owned_factory_;
};

}  // namespace cricket

#endif  // TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_
