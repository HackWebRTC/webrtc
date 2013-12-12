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

#include "talk/p2p/base/tcpport.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/p2p/base/common.h"

namespace cricket {

TCPPort::TCPPort(talk_base::Thread* thread,
                 talk_base::PacketSocketFactory* factory,
                 talk_base::Network* network, const talk_base::IPAddress& ip,
                 int min_port, int max_port, const std::string& username,
                 const std::string& password, bool allow_listen)
    : Port(thread, LOCAL_PORT_TYPE, factory, network, ip, min_port, max_port,
           username, password),
      incoming_only_(false),
      allow_listen_(allow_listen),
      socket_(NULL),
      error_(0) {
  // TODO(mallinath) - Set preference value as per RFC 6544.
  // http://b/issue?id=7141794
}

bool TCPPort::Init() {
  if (allow_listen_) {
    // Treat failure to create or bind a TCP socket as fatal.  This
    // should never happen.
    socket_ = socket_factory()->CreateServerTcpSocket(
        talk_base::SocketAddress(ip(), 0), min_port(), max_port(),
        false /* ssl */);
    if (!socket_) {
      LOG_J(LS_ERROR, this) << "TCP socket creation failed.";
      return false;
    }
    socket_->SignalNewConnection.connect(this, &TCPPort::OnNewConnection);
    socket_->SignalAddressReady.connect(this, &TCPPort::OnAddressReady);
  }
  return true;
}

TCPPort::~TCPPort() {
  delete socket_;
  std::list<Incoming>::iterator it;
  for (it = incoming_.begin(); it != incoming_.end(); ++it)
    delete it->socket;
  incoming_.clear();
}

Connection* TCPPort::CreateConnection(const Candidate& address,
                                      CandidateOrigin origin) {
  // We only support TCP protocols
  if ((address.protocol() != TCP_PROTOCOL_NAME) &&
      (address.protocol() != SSLTCP_PROTOCOL_NAME)) {
    return NULL;
  }

  // We can't accept TCP connections incoming on other ports
  if (origin == ORIGIN_OTHER_PORT)
    return NULL;

  // Check if we are allowed to make outgoing TCP connections
  if (incoming_only_ && (origin == ORIGIN_MESSAGE))
    return NULL;

  // We don't know how to act as an ssl server yet
  if ((address.protocol() == SSLTCP_PROTOCOL_NAME) &&
      (origin == ORIGIN_THIS_PORT)) {
    return NULL;
  }

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  TCPConnection* conn = NULL;
  if (talk_base::AsyncPacketSocket* socket =
      GetIncoming(address.address(), true)) {
    socket->SignalReadPacket.disconnect(this);
    conn = new TCPConnection(this, address, socket);
  } else {
    conn = new TCPConnection(this, address);
  }
  AddConnection(conn);
  return conn;
}

void TCPPort::PrepareAddress() {
  if (socket_) {
    // If socket isn't bound yet the address will be added in
    // OnAddressReady(). Socket may be in the CLOSED state if Listen()
    // failed, we still want ot add the socket address.
    LOG(LS_VERBOSE) << "Preparing TCP address, current state: "
                    << socket_->GetState();
    if (socket_->GetState() == talk_base::AsyncPacketSocket::STATE_BOUND ||
        socket_->GetState() == talk_base::AsyncPacketSocket::STATE_CLOSED)
      AddAddress(socket_->GetLocalAddress(), socket_->GetLocalAddress(),
                 TCP_PROTOCOL_NAME, LOCAL_PORT_TYPE,
                 ICE_TYPE_PREFERENCE_HOST_TCP, true);
  } else {
    LOG_J(LS_INFO, this) << "Not listening due to firewall restrictions.";
    // Note: We still add the address, since otherwise the remote side won't
    // recognize our incoming TCP connections.
    AddAddress(talk_base::SocketAddress(ip(), 0),
               talk_base::SocketAddress(ip(), 0), TCP_PROTOCOL_NAME,
               LOCAL_PORT_TYPE, ICE_TYPE_PREFERENCE_HOST_TCP, true);
  }
}

int TCPPort::SendTo(const void* data, size_t size,
                    const talk_base::SocketAddress& addr,
                    talk_base::DiffServCodePoint dscp,
                    bool payload) {
  talk_base::AsyncPacketSocket * socket = NULL;
  if (TCPConnection * conn = static_cast<TCPConnection*>(GetConnection(addr))) {
    socket = conn->socket();
  } else {
    socket = GetIncoming(addr);
  }
  if (!socket) {
    LOG_J(LS_ERROR, this) << "Attempted to send to an unknown destination, "
                          << addr.ToSensitiveString();
    return -1;  // TODO: Set error_
  }

  int sent = socket->Send(data, size, dscp);
  if (sent < 0) {
    error_ = socket->GetError();
    LOG_J(LS_ERROR, this) << "TCP send of " << size
                          << " bytes failed with error " << error_;
  }
  return sent;
}

int TCPPort::GetOption(talk_base::Socket::Option opt, int* value) {
  if (socket_) {
    return socket_->GetOption(opt, value);
  } else {
    return SOCKET_ERROR;
  }
}

int TCPPort::SetOption(talk_base::Socket::Option opt, int value) {
  // If we are setting DSCP value, pass value to base Port and return.
  // TODO(mallinath) - After we have the support on socket,
  // remove this specialization.
  if (opt == talk_base::Socket::OPT_DSCP) {
    SetDefaultDscpValue(static_cast<talk_base::DiffServCodePoint>(value));
    return 0;
  }

  if (socket_) {
    return socket_->SetOption(opt, value);
  } else {
    return SOCKET_ERROR;
  }
}

int TCPPort::GetError() {
  return error_;
}

void TCPPort::OnNewConnection(talk_base::AsyncPacketSocket* socket,
                              talk_base::AsyncPacketSocket* new_socket) {
  ASSERT(socket == socket_);

  Incoming incoming;
  incoming.addr = new_socket->GetRemoteAddress();
  incoming.socket = new_socket;
  incoming.socket->SignalReadPacket.connect(this, &TCPPort::OnReadPacket);
  incoming.socket->SignalReadyToSend.connect(this, &TCPPort::OnReadyToSend);

  LOG_J(LS_VERBOSE, this) << "Accepted connection from "
                          << incoming.addr.ToSensitiveString();
  incoming_.push_back(incoming);
}

talk_base::AsyncPacketSocket* TCPPort::GetIncoming(
    const talk_base::SocketAddress& addr, bool remove) {
  talk_base::AsyncPacketSocket* socket = NULL;
  for (std::list<Incoming>::iterator it = incoming_.begin();
       it != incoming_.end(); ++it) {
    if (it->addr == addr) {
      socket = it->socket;
      if (remove)
        incoming_.erase(it);
      break;
    }
  }
  return socket;
}

void TCPPort::OnReadPacket(talk_base::AsyncPacketSocket* socket,
                           const char* data, size_t size,
                           const talk_base::SocketAddress& remote_addr,
                           const talk_base::PacketTime& packet_time) {
  Port::OnReadPacket(data, size, remote_addr, PROTO_TCP);
}

void TCPPort::OnReadyToSend(talk_base::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

void TCPPort::OnAddressReady(talk_base::AsyncPacketSocket* socket,
                             const talk_base::SocketAddress& address) {
  AddAddress(address, address, "tcp",
             LOCAL_PORT_TYPE, ICE_TYPE_PREFERENCE_HOST_TCP,
             true);
}

TCPConnection::TCPConnection(TCPPort* port, const Candidate& candidate,
                             talk_base::AsyncPacketSocket* socket)
    : Connection(port, 0, candidate), socket_(socket), error_(0) {
  bool outgoing = (socket_ == NULL);
  if (outgoing) {
    // TODO: Handle failures here (unlikely since TCP).
    int opts = (candidate.protocol() == SSLTCP_PROTOCOL_NAME) ?
        talk_base::PacketSocketFactory::OPT_SSLTCP : 0;
    socket_ = port->socket_factory()->CreateClientTcpSocket(
        talk_base::SocketAddress(port_->Network()->ip(), 0),
        candidate.address(), port->proxy(), port->user_agent(), opts);
    if (socket_) {
      LOG_J(LS_VERBOSE, this) << "Connecting from "
                              << socket_->GetLocalAddress().ToSensitiveString()
                              << " to "
                              << candidate.address().ToSensitiveString();
      set_connected(false);
      socket_->SignalConnect.connect(this, &TCPConnection::OnConnect);
    } else {
      LOG_J(LS_WARNING, this) << "Failed to create connection to "
                              << candidate.address().ToSensitiveString();
    }
  } else {
    // Incoming connections should match the network address.
    ASSERT(socket_->GetLocalAddress().ipaddr() == port->ip());
  }

  if (socket_) {
    socket_->SignalReadPacket.connect(this, &TCPConnection::OnReadPacket);
    socket_->SignalReadyToSend.connect(this, &TCPConnection::OnReadyToSend);
    socket_->SignalClose.connect(this, &TCPConnection::OnClose);
  }
}

TCPConnection::~TCPConnection() {
  delete socket_;
}

int TCPConnection::Send(const void* data, size_t size,
                        talk_base::DiffServCodePoint dscp) {
  if (!socket_) {
    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }

  if (write_state() != STATE_WRITABLE) {
    // TODO: Should STATE_WRITE_TIMEOUT return a non-blocking error?
    error_ = EWOULDBLOCK;
    return SOCKET_ERROR;
  }
  int sent = socket_->Send(data, size, dscp);
  if (sent < 0) {
    error_ = socket_->GetError();
  } else {
    send_rate_tracker_.Update(sent);
  }
  return sent;
}

int TCPConnection::GetError() {
  return error_;
}

void TCPConnection::OnConnect(talk_base::AsyncPacketSocket* socket) {
  ASSERT(socket == socket_);
  LOG_J(LS_VERBOSE, this) << "Connection established to "
                          << socket->GetRemoteAddress().ToSensitiveString();
  set_connected(true);
}

void TCPConnection::OnClose(talk_base::AsyncPacketSocket* socket, int error) {
  ASSERT(socket == socket_);
  LOG_J(LS_VERBOSE, this) << "Connection closed with error " << error;
  set_connected(false);
  set_write_state(STATE_WRITE_TIMEOUT);
}

void TCPConnection::OnReadPacket(
  talk_base::AsyncPacketSocket* socket, const char* data, size_t size,
  const talk_base::SocketAddress& remote_addr,
  const talk_base::PacketTime& packet_time) {
  ASSERT(socket == socket_);
  Connection::OnReadPacket(data, size, packet_time);
}

void TCPConnection::OnReadyToSend(talk_base::AsyncPacketSocket* socket) {
  ASSERT(socket == socket_);
  Connection::OnReadyToSend();
}

}  // namespace cricket
