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

#ifndef TALK_EXAMPLES_LOGIN_AUTOPORTALLOCATOR_H_
#define TALK_EXAMPLES_LOGIN_AUTOPORTALLOCATOR_H_

#include <string>
#include <vector>

#include "talk/base/sigslot.h"
#include "talk/p2p/client/httpportallocator.h"
#include "talk/xmpp/jingleinfotask.h"
#include "talk/xmpp/xmppclient.h"

// This class sets the relay and stun servers using XmppClient.
// It enables the client to traverse Proxy and NAT.
class AutoPortAllocator : public cricket::HttpPortAllocator {
 public:
  AutoPortAllocator(talk_base::NetworkManager* network_manager,
                    const std::string& user_agent)
      : cricket::HttpPortAllocator(network_manager, user_agent) {
  }

  // Creates and initiates a task to get relay token from XmppClient and set
  // it appropriately.
  void SetXmppClient(buzz::XmppClient* client) {
    // The JingleInfoTask is freed by the task-runner.
    buzz::JingleInfoTask* jit = new buzz::JingleInfoTask(client);
    jit->SignalJingleInfo.connect(this, &AutoPortAllocator::OnJingleInfo);
    jit->Start();
    jit->RefreshJingleInfoNow();
  }

 private:
  void OnJingleInfo(
      const std::string& token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<talk_base::SocketAddress>& stun_hosts) {
    SetRelayToken(token);
    SetStunHosts(stun_hosts);
    SetRelayHosts(relay_hosts);
  }
};

#endif  // TALK_EXAMPLES_LOGIN_AUTOPORTALLOCATOR_H_
