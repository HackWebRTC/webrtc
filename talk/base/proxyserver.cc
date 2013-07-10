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

#include "talk/base/proxyserver.h"

#include <algorithm>
#include "talk/base/socketfactory.h"

namespace talk_base {

// ProxyServer
ProxyServer::ProxyServer(
    SocketFactory* int_factory, const SocketAddress& int_addr,
    SocketFactory* ext_factory, const SocketAddress& ext_ip)
    : ext_factory_(ext_factory), ext_ip_(ext_ip.ipaddr(), 0),  // strip off port
      server_socket_(int_factory->CreateAsyncSocket(int_addr.family(),
                                                    SOCK_STREAM)) {
  ASSERT(server_socket_.get() != NULL);
  ASSERT(int_addr.family() == AF_INET || int_addr.family() == AF_INET6);
  server_socket_->Bind(int_addr);
  server_socket_->Listen(5);
  server_socket_->SignalReadEvent.connect(this, &ProxyServer::OnAcceptEvent);
}

ProxyServer::~ProxyServer() {
  for (BindingList::iterator it = bindings_.begin();
       it != bindings_.end(); ++it) {
    delete (*it);
  }
}

void ProxyServer::OnAcceptEvent(AsyncSocket* socket) {
  ASSERT(socket != NULL && socket == server_socket_.get());
  AsyncSocket* int_socket = socket->Accept(NULL);
  AsyncProxyServerSocket* wrapped_socket = WrapSocket(int_socket);
  AsyncSocket* ext_socket = ext_factory_->CreateAsyncSocket(ext_ip_.family(),
                                                            SOCK_STREAM);
  if (ext_socket) {
    ext_socket->Bind(ext_ip_);
    bindings_.push_back(new ProxyBinding(wrapped_socket, ext_socket));
  } else {
    LOG(LS_ERROR) << "Unable to create external socket on proxy accept event";
  }
}

void ProxyServer::OnBindingDestroyed(ProxyBinding* binding) {
  BindingList::iterator it =
      std::find(bindings_.begin(), bindings_.end(), binding);
  delete (*it);
  bindings_.erase(it);
}

// ProxyBinding
ProxyBinding::ProxyBinding(AsyncProxyServerSocket* int_socket,
                           AsyncSocket* ext_socket)
    : int_socket_(int_socket), ext_socket_(ext_socket), connected_(false),
      out_buffer_(kBufferSize), in_buffer_(kBufferSize) {
  int_socket_->SignalConnectRequest.connect(this,
                                            &ProxyBinding::OnConnectRequest);
  int_socket_->SignalReadEvent.connect(this, &ProxyBinding::OnInternalRead);
  int_socket_->SignalWriteEvent.connect(this, &ProxyBinding::OnInternalWrite);
  int_socket_->SignalCloseEvent.connect(this, &ProxyBinding::OnInternalClose);
  ext_socket_->SignalConnectEvent.connect(this,
                                          &ProxyBinding::OnExternalConnect);
  ext_socket_->SignalReadEvent.connect(this, &ProxyBinding::OnExternalRead);
  ext_socket_->SignalWriteEvent.connect(this, &ProxyBinding::OnExternalWrite);
  ext_socket_->SignalCloseEvent.connect(this, &ProxyBinding::OnExternalClose);
}

void ProxyBinding::OnConnectRequest(AsyncProxyServerSocket* socket,
                                   const SocketAddress& addr) {
  ASSERT(!connected_ && ext_socket_.get() != NULL);
  ext_socket_->Connect(addr);
  // TODO: handle errors here
}

void ProxyBinding::OnInternalRead(AsyncSocket* socket) {
  Read(int_socket_.get(), &out_buffer_);
  Write(ext_socket_.get(), &out_buffer_);
}

void ProxyBinding::OnInternalWrite(AsyncSocket* socket) {
  Write(int_socket_.get(), &in_buffer_);
}

void ProxyBinding::OnInternalClose(AsyncSocket* socket, int err) {
  Destroy();
}

void ProxyBinding::OnExternalConnect(AsyncSocket* socket) {
  ASSERT(socket != NULL);
  connected_ = true;
  int_socket_->SendConnectResult(0, socket->GetRemoteAddress());
}

void ProxyBinding::OnExternalRead(AsyncSocket* socket) {
  Read(ext_socket_.get(), &in_buffer_);
  Write(int_socket_.get(), &in_buffer_);
}

void ProxyBinding::OnExternalWrite(AsyncSocket* socket) {
  Write(ext_socket_.get(), &out_buffer_);
}

void ProxyBinding::OnExternalClose(AsyncSocket* socket, int err) {
  if (!connected_) {
    int_socket_->SendConnectResult(err, SocketAddress());
  }
  Destroy();
}

void ProxyBinding::Read(AsyncSocket* socket, FifoBuffer* buffer) {
  // Only read if the buffer is empty.
  ASSERT(socket != NULL);
  size_t size;
  int read;
  if (buffer->GetBuffered(&size) && size == 0) {
    void* p = buffer->GetWriteBuffer(&size);
    read = socket->Recv(p, size);
    buffer->ConsumeWriteBuffer(_max(read, 0));
  }
}

void ProxyBinding::Write(AsyncSocket* socket, FifoBuffer* buffer) {
  ASSERT(socket != NULL);
  size_t size;
  int written;
  const void* p = buffer->GetReadData(&size);
  written = socket->Send(p, size);
  buffer->ConsumeReadData(_max(written, 0));
}

void ProxyBinding::Destroy() {
  SignalDestroyed(this);
}

}  // namespace talk_base
