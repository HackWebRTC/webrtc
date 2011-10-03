// Copyright 2010 Google Inc. All Rights Reserved,
//
// Author: Justin Uberti (juberti@google.com)

#ifndef TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_
#define TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_

#include <string>
#include "talk/base/basicpacketsocketfactory.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/udpport.h"

namespace talk_base {
class SocketFactory;
class Thread;
}

namespace cricket {

class FakePortAllocatorSession : public PortAllocatorSession {
 public:
  FakePortAllocatorSession(talk_base::Thread* worker_thread,
                           talk_base::PacketSocketFactory* factory,
                           const std::string& name,
                           const std::string& session_type)
      : PortAllocatorSession(0), worker_thread_(worker_thread),
        factory_(factory), name_(name),
        network_("network", "unittest", 0x7F000001, 0),
        port_(NULL), running_(false)  {
  }

  virtual void GetInitialPorts() {
    if (!port_.get()) {
      port_.reset(cricket::UDPPort::Create(worker_thread_, factory_,
                                           &network_, network_.ip(), 0, 0));
      AddPort(port_.get());
    }
  }
  virtual void StartGetAllPorts() { running_ = true; }
  virtual void StopGetAllPorts() { running_ = false; }
  virtual bool IsGettingAllPorts() { return running_; }

  void AddPort(cricket::Port* port) {
    port->set_name(name_);
    port->set_preference(1.0);
    port->set_generation(0);
    port->SignalAddressReady.connect(
        this, &FakePortAllocatorSession::OnAddressReady);
    port->PrepareAddress();
    SignalPortReady(this, port);
  }
  void OnAddressReady(cricket::Port* port) {
    SignalCandidatesReady(this, port->candidates());
  }

 private:
  talk_base::Thread* worker_thread_;
  talk_base::PacketSocketFactory* factory_;
  std::string name_;
  talk_base::Network network_;
  talk_base::scoped_ptr<cricket::Port> port_;
  bool running_;
};

class FakePortAllocator : public cricket::PortAllocator {
 public:
  FakePortAllocator(talk_base::Thread* worker_thread,
                    talk_base::PacketSocketFactory* factory)
      : worker_thread_(worker_thread), factory_(factory) {
    if (factory_ == NULL) {
      owned_factory_.reset(new talk_base::BasicPacketSocketFactory(
          worker_thread_));
      factory_ = owned_factory_.get();
    }
  }

  virtual cricket::PortAllocatorSession* CreateSession(
      const std::string &name, const std::string &session_type) {
    return new FakePortAllocatorSession(worker_thread_, factory_, name,
                                        session_type);
  }

 private:
  talk_base::Thread* worker_thread_;
  talk_base::PacketSocketFactory* factory_;
  talk_base::scoped_ptr<talk_base::BasicPacketSocketFactory> owned_factory_;
};

}  // namespace cricket

#endif  // TALK_P2P_CLIENT_FAKEPORTALLOCATOR_H_
