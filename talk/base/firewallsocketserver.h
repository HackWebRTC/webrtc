/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_BASE_FIREWALLSOCKETSERVER_H_
#define TALK_BASE_FIREWALLSOCKETSERVER_H_

#include <vector>
#include "talk/base/socketserver.h"
#include "talk/base/criticalsection.h"

namespace talk_base {

class FirewallManager;

// This SocketServer shim simulates a rule-based firewall server.

enum FirewallProtocol { FP_UDP, FP_TCP, FP_ANY };
enum FirewallDirection { FD_IN, FD_OUT, FD_ANY };

class FirewallSocketServer : public SocketServer {
 public:
  FirewallSocketServer(SocketServer * server,
                       FirewallManager * manager = NULL,
                       bool should_delete_server = false);
  virtual ~FirewallSocketServer();

  SocketServer* socketserver() const { return server_; }
  void set_socketserver(SocketServer* server) {
    if (server_ && should_delete_server_) {
      delete server_;
      server_ = NULL;
      should_delete_server_ = false;
    }
    server_ = server;
  }

  // Settings to control whether CreateSocket or Socket::Listen succeed.
  void set_udp_sockets_enabled(bool enabled) { udp_sockets_enabled_ = enabled; }
  void set_tcp_sockets_enabled(bool enabled) { tcp_sockets_enabled_ = enabled; }
  bool tcp_listen_enabled() const { return tcp_listen_enabled_; }
  void set_tcp_listen_enabled(bool enabled) { tcp_listen_enabled_ = enabled; }

  // Rules govern the behavior of Connect/Accept/Send/Recv attempts.
  void AddRule(bool allow, FirewallProtocol p = FP_ANY,
               FirewallDirection d = FD_ANY,
               const SocketAddress& addr = SocketAddress());
  void AddRule(bool allow, FirewallProtocol p,
               const SocketAddress& src, const SocketAddress& dst);
  void ClearRules();

  bool Check(FirewallProtocol p,
             const SocketAddress& src, const SocketAddress& dst);

  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  virtual void SetMessageQueue(MessageQueue* queue) {
    server_->SetMessageQueue(queue);
  }
  virtual bool Wait(int cms, bool process_io) {
    return server_->Wait(cms, process_io);
  }
  virtual void WakeUp() {
    return server_->WakeUp();
  }

  Socket * WrapSocket(Socket * sock, int type);
  AsyncSocket * WrapSocket(AsyncSocket * sock, int type);

 private:
  SocketServer * server_;
  FirewallManager * manager_;
  CriticalSection crit_;
  struct Rule {
    bool allow;
    FirewallProtocol p;
    FirewallDirection d;
    SocketAddress src;
    SocketAddress dst;
  };
  std::vector<Rule> rules_;
  bool should_delete_server_;
  bool udp_sockets_enabled_;
  bool tcp_sockets_enabled_;
  bool tcp_listen_enabled_;
};

// FirewallManager allows you to manage firewalls in multiple threads together

class FirewallManager {
 public:
  FirewallManager();
  ~FirewallManager();

  void AddServer(FirewallSocketServer * server);
  void RemoveServer(FirewallSocketServer * server);

  void AddRule(bool allow, FirewallProtocol p = FP_ANY,
               FirewallDirection d = FD_ANY,
               const SocketAddress& addr = SocketAddress());
  void ClearRules();

 private:
  CriticalSection crit_;
  std::vector<FirewallSocketServer *> servers_;
};

}  // namespace talk_base

#endif  // TALK_BASE_FIREWALLSOCKETSERVER_H_
