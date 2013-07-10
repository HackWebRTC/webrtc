/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_P2P_BASE_TESTTURNSERVER_H_
#define TALK_P2P_BASE_TESTTURNSERVER_H_

#include <string>

#include "talk/base/asyncudpsocket.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/stun.h"
#include "talk/p2p/base/turnserver.h"

namespace cricket {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

class TestTurnServer : public TurnAuthInterface {
 public:
  TestTurnServer(talk_base::Thread* thread,
                 const talk_base::SocketAddress& udp_int_addr,
                 const talk_base::SocketAddress& udp_ext_addr)
      : server_(thread) {
    server_.AddInternalSocket(talk_base::AsyncUDPSocket::Create(
        thread->socketserver(), udp_int_addr), PROTO_UDP);
    server_.SetExternalSocketFactory(new talk_base::BasicPacketSocketFactory(),
        udp_ext_addr);
    server_.set_realm(kTestRealm);
    server_.set_software(kTestSoftware);
    server_.set_auth_hook(this);
  }

  void set_enable_otu_nonce(bool enable) {
    server_.set_enable_otu_nonce(enable);
  }

  TurnServer* server() { return &server_; }

 private:
  // For this test server, succeed if the password is the same as the username.
  // Obviously, do not use this in a production environment.
  virtual bool GetKey(const std::string& username, const std::string& realm,
                      std::string* key) {
    return ComputeStunCredentialHash(username, realm, username, key);
  }

  TurnServer server_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TESTTURNSERVER_H_
