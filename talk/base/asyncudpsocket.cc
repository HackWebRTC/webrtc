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

#include "talk/base/asyncudpsocket.h"
#include "talk/base/logging.h"

namespace talk_base {

static const int BUF_SIZE = 64 * 1024;

AsyncUDPSocket* AsyncUDPSocket::Create(
    AsyncSocket* socket,
    const SocketAddress& bind_address) {
  scoped_ptr<AsyncSocket> owned_socket(socket);
  if (socket->Bind(bind_address) < 0) {
    LOG(LS_ERROR) << "Bind() failed with error " << socket->GetError();
    return NULL;
  }
  return new AsyncUDPSocket(owned_socket.release());
}

AsyncUDPSocket* AsyncUDPSocket::Create(SocketFactory* factory,
                                       const SocketAddress& bind_address) {
  AsyncSocket* socket =
      factory->CreateAsyncSocket(bind_address.family(), SOCK_DGRAM);
  if (!socket)
    return NULL;
  return Create(socket, bind_address);
}

AsyncUDPSocket::AsyncUDPSocket(AsyncSocket* socket)
    : socket_(socket) {
  ASSERT(socket_);
  size_ = BUF_SIZE;
  buf_ = new char[size_];

  // The socket should start out readable but not writable.
  socket_->SignalReadEvent.connect(this, &AsyncUDPSocket::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &AsyncUDPSocket::OnWriteEvent);
}

AsyncUDPSocket::~AsyncUDPSocket() {
  delete [] buf_;
}

SocketAddress AsyncUDPSocket::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

SocketAddress AsyncUDPSocket::GetRemoteAddress() const {
  return socket_->GetRemoteAddress();
}

// TODO(mallinath) - Add support of setting DSCP code on AsyncSocket.
int AsyncUDPSocket::Send(const void *pv, size_t cb, DiffServCodePoint dscp) {
  return socket_->Send(pv, cb);
}

// TODO(mallinath) - Add support of setting DSCP code on AsyncSocket.
int AsyncUDPSocket::SendTo(const void *pv, size_t cb,
                           const SocketAddress& addr, DiffServCodePoint dscp) {
  return socket_->SendTo(pv, cb, addr);
}

int AsyncUDPSocket::Close() {
  return socket_->Close();
}

AsyncUDPSocket::State AsyncUDPSocket::GetState() const {
  return STATE_BOUND;
}

int AsyncUDPSocket::GetOption(Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int AsyncUDPSocket::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int AsyncUDPSocket::GetError() const {
  return socket_->GetError();
}

void AsyncUDPSocket::SetError(int error) {
  return socket_->SetError(error);
}

void AsyncUDPSocket::OnReadEvent(AsyncSocket* socket) {
  ASSERT(socket_.get() == socket);

  SocketAddress remote_addr;
  int len = socket_->RecvFrom(buf_, size_, &remote_addr);
  if (len < 0) {
    // An error here typically means we got an ICMP error in response to our
    // send datagram, indicating the remote address was unreachable.
    // When doing ICE, this kind of thing will often happen.
    // TODO: Do something better like forwarding the error to the user.
    SocketAddress local_addr = socket_->GetLocalAddress();
    LOG(LS_INFO) << "AsyncUDPSocket[" << local_addr.ToSensitiveString() << "] "
                 << "receive failed with error " << socket_->GetError();
    return;
  }

  // TODO: Make sure that we got all of the packet.
  // If we did not, then we should resize our buffer to be large enough.
  SignalReadPacket(this, buf_, (size_t)len, remote_addr);
}

void AsyncUDPSocket::OnWriteEvent(AsyncSocket* socket) {
  SignalReadyToSend(this);
}

}  // namespace talk_base
