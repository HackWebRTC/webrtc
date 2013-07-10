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

#ifndef TALK_BASE_BASICPACKETSOCKETFACTORY_H_
#define TALK_BASE_BASICPACKETSOCKETFACTORY_H_

#include "talk/p2p/base/packetsocketfactory.h"

namespace talk_base {

class AsyncSocket;
class SocketFactory;
class Thread;

class BasicPacketSocketFactory : public PacketSocketFactory {
 public:
  BasicPacketSocketFactory();
  explicit BasicPacketSocketFactory(Thread* thread);
  explicit BasicPacketSocketFactory(SocketFactory* socket_factory);
  virtual ~BasicPacketSocketFactory();

  virtual AsyncPacketSocket* CreateUdpSocket(
      const SocketAddress& local_address, int min_port, int max_port);
  virtual AsyncPacketSocket* CreateServerTcpSocket(
      const SocketAddress& local_address, int min_port, int max_port, int opts);
  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address, const SocketAddress& remote_address,
      const ProxyInfo& proxy_info, const std::string& user_agent, int opts);

 private:
  int BindSocket(AsyncSocket* socket, const SocketAddress& local_address,
                 int min_port, int max_port);

  SocketFactory* socket_factory();

  Thread* thread_;
  SocketFactory* socket_factory_;
};

}  // namespace talk_base

#endif  // TALK_BASE_BASICPACKETSOCKETFACTORY_H_
