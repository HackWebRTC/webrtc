/*
 * libjingle
 * Copyright 2009 Google Inc.
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

#ifndef TALK_BASE_FAKENETWORK_H_
#define TALK_BASE_FAKENETWORK_H_

#include <string>
#include <vector>

#include "talk/base/network.h"
#include "talk/base/messagehandler.h"
#include "talk/base/socketaddress.h"
#include "talk/base/stringencode.h"
#include "talk/base/thread.h"

namespace talk_base {

const int kFakeIPv4NetworkPrefixLength = 24;
const int kFakeIPv6NetworkPrefixLength = 64;

// Fake network manager that allows us to manually specify the IPs to use.
class FakeNetworkManager : public NetworkManagerBase,
                           public MessageHandler {
 public:
  FakeNetworkManager()
      : thread_(Thread::Current()),
        next_index_(0),
        started_(false),
        sent_first_update_(false) {
  }

  typedef std::vector<SocketAddress> IfaceList;

  void AddInterface(const SocketAddress& iface) {
    // ensure a unique name for the interface
    SocketAddress address("test" + talk_base::ToString(next_index_++), 0);
    address.SetResolvedIP(iface.ipaddr());
    ifaces_.push_back(address);
    DoUpdateNetworks();
  }

  void RemoveInterface(const SocketAddress& iface) {
    for (IfaceList::iterator it = ifaces_.begin();
         it != ifaces_.end(); ++it) {
      if (it->EqualIPs(iface)) {
        ifaces_.erase(it);
        break;
      }
    }
    DoUpdateNetworks();
  }

  virtual void StartUpdating() {
    if (started_) {
      if (sent_first_update_)
        SignalNetworksChanged();
      return;
    }

    started_ = true;
    sent_first_update_ = false;
    thread_->Post(this);
  }

  virtual void StopUpdating() {
    started_ = false;
  }

  // MessageHandler interface.
  virtual void OnMessage(Message* msg) {
    DoUpdateNetworks();
  }

 private:
  void DoUpdateNetworks() {
    if (!started_)
      return;
    std::vector<Network*> networks;
    for (IfaceList::iterator it = ifaces_.begin();
         it != ifaces_.end(); ++it) {
      int prefix_length = 0;
      if (it->ipaddr().family() == AF_INET) {
        prefix_length = kFakeIPv4NetworkPrefixLength;
      } else if (it->ipaddr().family() == AF_INET6) {
        prefix_length = kFakeIPv6NetworkPrefixLength;
      }
      IPAddress prefix = TruncateIP(it->ipaddr(), prefix_length);
      scoped_ptr<Network> net(new Network(it->hostname(),
                                          it->hostname(),
                                          prefix,
                                          prefix_length));
      net->AddIP(it->ipaddr());
      networks.push_back(net.release());
    }
    bool changed;
    MergeNetworkList(networks, &changed);
    if (changed || !sent_first_update_) {
      SignalNetworksChanged();
      sent_first_update_ = true;
    }
  }

  Thread* thread_;
  IfaceList ifaces_;
  int next_index_;
  bool started_;
  bool sent_first_update_;
};

}  // namespace talk_base

#endif  // TALK_BASE_FAKENETWORK_H_
