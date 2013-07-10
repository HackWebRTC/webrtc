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

#ifndef TALK_BASE_PROXYSERVER_H_
#define TALK_BASE_PROXYSERVER_H_

#include <list>
#include "talk/base/asyncsocket.h"
#include "talk/base/socketadapters.h"
#include "talk/base/socketaddress.h"
#include "talk/base/stream.h"

namespace talk_base {

class SocketFactory;

// ProxyServer is a base class that allows for easy construction of proxy
// servers. With its helper class ProxyBinding, it contains all the necessary
// logic for receiving and bridging connections. The specific client-server
// proxy protocol is implemented by an instance of the AsyncProxyServerSocket
// class; children of ProxyServer implement WrapSocket appropriately to return
// the correct protocol handler.

class ProxyBinding : public sigslot::has_slots<> {
 public:
  ProxyBinding(AsyncProxyServerSocket* in_socket, AsyncSocket* out_socket);
  sigslot::signal1<ProxyBinding*> SignalDestroyed;

 private:
  void OnConnectRequest(AsyncProxyServerSocket* socket,
                        const SocketAddress& addr);
  void OnInternalRead(AsyncSocket* socket);
  void OnInternalWrite(AsyncSocket* socket);
  void OnInternalClose(AsyncSocket* socket, int err);
  void OnExternalConnect(AsyncSocket* socket);
  void OnExternalRead(AsyncSocket* socket);
  void OnExternalWrite(AsyncSocket* socket);
  void OnExternalClose(AsyncSocket* socket, int err);

  static void Read(AsyncSocket* socket, FifoBuffer* buffer);
  static void Write(AsyncSocket* socket, FifoBuffer* buffer);
  void Destroy();

  static const int kBufferSize = 4096;
  scoped_ptr<AsyncProxyServerSocket> int_socket_;
  scoped_ptr<AsyncSocket> ext_socket_;
  bool connected_;
  FifoBuffer out_buffer_;
  FifoBuffer in_buffer_;
  DISALLOW_EVIL_CONSTRUCTORS(ProxyBinding);
};

class ProxyServer : public sigslot::has_slots<> {
 public:
  ProxyServer(SocketFactory* int_factory, const SocketAddress& int_addr,
              SocketFactory* ext_factory, const SocketAddress& ext_ip);
  virtual ~ProxyServer();

 protected:
  void OnAcceptEvent(AsyncSocket* socket);
  virtual AsyncProxyServerSocket* WrapSocket(AsyncSocket* socket) = 0;
  void OnBindingDestroyed(ProxyBinding* binding);

 private:
  typedef std::list<ProxyBinding*> BindingList;
  SocketFactory* ext_factory_;
  SocketAddress ext_ip_;
  scoped_ptr<AsyncSocket> server_socket_;
  BindingList bindings_;
  DISALLOW_EVIL_CONSTRUCTORS(ProxyServer);
};

// SocksProxyServer is a simple extension of ProxyServer to implement SOCKS.
class SocksProxyServer : public ProxyServer {
 public:
  SocksProxyServer(SocketFactory* int_factory, const SocketAddress& int_addr,
                   SocketFactory* ext_factory, const SocketAddress& ext_ip)
      : ProxyServer(int_factory, int_addr, ext_factory, ext_ip) {
  }
 protected:
  AsyncProxyServerSocket* WrapSocket(AsyncSocket* socket) {
    return new AsyncSocksProxyServerSocket(socket);
  }
  DISALLOW_EVIL_CONSTRUCTORS(SocksProxyServer);
};

}  // namespace talk_base

#endif  // TALK_BASE_PROXYSERVER_H_
