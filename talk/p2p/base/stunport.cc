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

#include "talk/p2p/base/stunport.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/helpers.h"
#include "talk/base/nethelpers.h"
#include "talk/p2p/base/common.h"
#include "talk/p2p/base/stun.h"

namespace cricket {

// TODO: Move these to a common place (used in relayport too)
const int KEEPALIVE_DELAY = 10 * 1000;  // 10 seconds - sort timeouts
const int RETRY_DELAY = 50;             // 50ms, from ICE spec
const int RETRY_TIMEOUT = 50 * 1000;    // ICE says 50 secs

// Handles a binding request sent to the STUN server.
class StunBindingRequest : public StunRequest {
 public:
  StunBindingRequest(UDPPort* port, bool keep_alive,
                     const talk_base::SocketAddress& addr)
    : port_(port), keep_alive_(keep_alive), server_addr_(addr) {
    start_time_ = talk_base::Time();
  }

  virtual ~StunBindingRequest() {
  }

  const talk_base::SocketAddress& server_addr() const { return server_addr_; }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
  }

  virtual void OnResponse(StunMessage* response) {
    const StunAddressAttribute* addr_attr =
        response->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
    if (!addr_attr) {
      LOG(LS_ERROR) << "Binding response missing mapped address.";
    } else if (addr_attr->family() != STUN_ADDRESS_IPV4 &&
               addr_attr->family() != STUN_ADDRESS_IPV6) {
      LOG(LS_ERROR) << "Binding address has bad family";
    } else {
      talk_base::SocketAddress addr(addr_attr->ipaddr(), addr_attr->port());
      port_->OnStunBindingRequestSucceeded(addr);
    }

    // We will do a keep-alive regardless of whether this request suceeds.
    // This should have almost no impact on network usage.
    if (keep_alive_) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          port_->stun_keepalive_delay());
    }
  }

  virtual void OnErrorResponse(StunMessage* response) {
    const StunErrorCodeAttribute* attr = response->GetErrorCode();
    if (!attr) {
      LOG(LS_ERROR) << "Bad allocate response error code";
    } else {
      LOG(LS_ERROR) << "Binding error response:"
                 << " class=" << attr->eclass()
                 << " number=" << attr->number()
                 << " reason='" << attr->reason() << "'";
    }

    port_->OnStunBindingOrResolveRequestFailed();

    if (keep_alive_
        && (talk_base::TimeSince(start_time_) <= RETRY_TIMEOUT)) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          port_->stun_keepalive_delay());
    }
  }

  virtual void OnTimeout() {
    LOG(LS_ERROR) << "Binding request timed out from "
      << port_->GetLocalAddress().ToSensitiveString()
      << " (" << port_->Network()->name() << ")";

    port_->OnStunBindingOrResolveRequestFailed();

    if (keep_alive_
        && (talk_base::TimeSince(start_time_) <= RETRY_TIMEOUT)) {
      port_->requests_.SendDelayed(
          new StunBindingRequest(port_, true, server_addr_),
          RETRY_DELAY);
    }
  }

 private:
  UDPPort* port_;
  bool keep_alive_;
  talk_base::SocketAddress server_addr_;
  uint32 start_time_;
};

UDPPort::UDPPort(talk_base::Thread* thread,
                 talk_base::PacketSocketFactory* factory,
                 talk_base::Network* network,
                 talk_base::AsyncPacketSocket* socket,
                 const std::string& username, const std::string& password)
    : Port(thread, factory, network, socket->GetLocalAddress().ipaddr(),
           username, password),
      requests_(thread),
      socket_(socket),
      error_(0),
      resolver_(NULL),
      ready_(false),
      stun_keepalive_delay_(KEEPALIVE_DELAY) {
}

UDPPort::UDPPort(talk_base::Thread* thread,
                 talk_base::PacketSocketFactory* factory,
                 talk_base::Network* network,
                 const talk_base::IPAddress& ip, int min_port, int max_port,
                 const std::string& username, const std::string& password)
    : Port(thread, LOCAL_PORT_TYPE, factory, network, ip, min_port, max_port,
           username, password),
      requests_(thread),
      socket_(NULL),
      error_(0),
      resolver_(NULL),
      ready_(false),
      stun_keepalive_delay_(KEEPALIVE_DELAY) {
}

bool UDPPort::Init() {
  if (!SharedSocket()) {
    ASSERT(socket_ == NULL);
    socket_ = socket_factory()->CreateUdpSocket(
        talk_base::SocketAddress(ip(), 0), min_port(), max_port());
    if (!socket_) {
      LOG_J(LS_WARNING, this) << "UDP socket creation failed";
      return false;
    }
    socket_->SignalReadPacket.connect(this, &UDPPort::OnReadPacket);
  }
  socket_->SignalReadyToSend.connect(this, &UDPPort::OnReadyToSend);
  socket_->SignalAddressReady.connect(this, &UDPPort::OnLocalAddressReady);
  requests_.SignalSendPacket.connect(this, &UDPPort::OnSendPacket);
  return true;
}

UDPPort::~UDPPort() {
  if (resolver_) {
    resolver_->Destroy(false);
  }
  if (!SharedSocket())
    delete socket_;
}

void UDPPort::PrepareAddress() {
  ASSERT(requests_.empty());
  if (socket_->GetState() == talk_base::AsyncPacketSocket::STATE_BOUND) {
    OnLocalAddressReady(socket_, socket_->GetLocalAddress());
  }
}

void UDPPort::MaybePrepareStunCandidate() {
  // Sending binding request to the STUN server if address is available to
  // prepare STUN candidate.
  if (!server_addr_.IsNil()) {
    SendStunBindingRequest();
  } else {
    // Processing host candidate address.
    SetResult(true);
  }
}

Connection* UDPPort::CreateConnection(const Candidate& address,
                                       CandidateOrigin origin) {
  if (address.protocol() != "udp")
    return NULL;

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  if (SharedSocket() && Candidates()[0].type() != LOCAL_PORT_TYPE) {
    ASSERT(false);
    return NULL;
  }

  Connection* conn = new ProxyConnection(this, 0, address);
  AddConnection(conn);
  return conn;
}

int UDPPort::SendTo(const void* data, size_t size,
                    const talk_base::SocketAddress& addr,
                    talk_base::DiffServCodePoint dscp,
                    bool payload) {
  int sent = socket_->SendTo(data, size, addr, dscp);
  if (sent < 0) {
    error_ = socket_->GetError();
    LOG_J(LS_ERROR, this) << "UDP send of " << size
                          << " bytes failed with error " << error_;
  }
  return sent;
}

int UDPPort::SetOption(talk_base::Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int UDPPort::GetOption(talk_base::Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int UDPPort::GetError() {
  return error_;
}

void UDPPort::OnLocalAddressReady(talk_base::AsyncPacketSocket* socket,
                                  const talk_base::SocketAddress& address) {
  AddAddress(address, address, UDP_PROTOCOL_NAME, LOCAL_PORT_TYPE,
             ICE_TYPE_PREFERENCE_HOST, false);
  MaybePrepareStunCandidate();
}

void UDPPort::OnReadPacket(
  talk_base::AsyncPacketSocket* socket, const char* data, size_t size,
  const talk_base::SocketAddress& remote_addr,
  const talk_base::PacketTime& packet_time) {
  ASSERT(socket == socket_);

  // Look for a response from the STUN server.
  // Even if the response doesn't match one of our outstanding requests, we
  // will eat it because it might be a response to a retransmitted packet, and
  // we already cleared the request when we got the first response.
  if (!server_addr_.IsUnresolved() && remote_addr == server_addr_) {
    requests_.CheckResponse(data, size);
    return;
  }

  if (Connection* conn = GetConnection(remote_addr)) {
    conn->OnReadPacket(data, size, packet_time);
  } else {
    Port::OnReadPacket(data, size, remote_addr, PROTO_UDP);
  }
}

void UDPPort::OnReadyToSend(talk_base::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

void UDPPort::SendStunBindingRequest() {
  // We will keep pinging the stun server to make sure our NAT pin-hole stays
  // open during the call.
  // TODO: Support multiple stun servers, or make ResolveStunAddress find a
  // server with the correct family, or something similar.
  ASSERT(requests_.empty());
  if (server_addr_.IsUnresolved()) {
    ResolveStunAddress();
  } else if (socket_->GetState() == talk_base::AsyncPacketSocket::STATE_BOUND) {
    // Check if |server_addr_| is compatible with the port's ip.
    if (IsCompatibleAddress(server_addr_)) {
      requests_.Send(new StunBindingRequest(this, true, server_addr_));
    } else {
      // Since we can't send stun messages to the server, we should mark this
      // port ready.
      OnStunBindingOrResolveRequestFailed();
    }
  }
}

void UDPPort::ResolveStunAddress() {
  if (resolver_)
    return;

  resolver_ = socket_factory()->CreateAsyncResolver();
  resolver_->SignalDone.connect(this, &UDPPort::OnResolveResult);
  resolver_->Start(server_addr_);
}

void UDPPort::OnResolveResult(talk_base::AsyncResolverInterface* resolver) {
  ASSERT(resolver == resolver_);
  if (resolver_->GetError() != 0 ||
      !resolver_->GetResolvedAddress(ip().family(), &server_addr_))  {
    LOG_J(LS_WARNING, this) << "StunPort: stun host lookup received error "
                            << resolver_->GetError();
    OnStunBindingOrResolveRequestFailed();
    return;
  }

  SendStunBindingRequest();
}

void UDPPort::OnStunBindingRequestSucceeded(
    const talk_base::SocketAddress& stun_addr) {
  if (ready_)  // Discarding the binding response if port is already enabled.
    return;

  if (!SharedSocket() || stun_addr != socket_->GetLocalAddress()) {
    // If socket is shared and |stun_addr| is equal to local socket
    // address then discarding the stun address.
    // Setting related address before STUN candidate is added. For STUN
    // related address is local socket address.
    set_related_address(socket_->GetLocalAddress());
    AddAddress(stun_addr, socket_->GetLocalAddress(), UDP_PROTOCOL_NAME,
               STUN_PORT_TYPE, ICE_TYPE_PREFERENCE_SRFLX, false);
  }
  SetResult(true);
}

void UDPPort::OnStunBindingOrResolveRequestFailed() {
  if (ready_)  // Discarding failure response if port is already enabled.
    return;

  // If socket is shared, we should process local udp candidate.
  SetResult(SharedSocket());
}

void UDPPort::SetResult(bool success) {
  // Setting ready status.
  ready_ = true;
  if (success) {
    SignalPortComplete(this);
  } else {
    SignalPortError(this);
  }
}

// TODO: merge this with SendTo above.
void UDPPort::OnSendPacket(const void* data, size_t size, StunRequest* req) {
  StunBindingRequest* sreq = static_cast<StunBindingRequest*>(req);
  if (socket_->SendTo(data, size, sreq->server_addr(), DefaultDscpValue()) < 0)
    PLOG(LERROR, socket_->GetError()) << "sendto";
}

}  // namespace cricket
