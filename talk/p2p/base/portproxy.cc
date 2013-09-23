/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/p2p/base/portproxy.h"

namespace cricket {

void PortProxy::set_impl(PortInterface* port) {
  impl_ = port;
  impl_->SignalUnknownAddress.connect(
      this, &PortProxy::OnUnknownAddress);
  impl_->SignalDestroyed.connect(this, &PortProxy::OnPortDestroyed);
  impl_->SignalRoleConflict.connect(this, &PortProxy::OnRoleConflict);
}

const std::string& PortProxy::Type() const {
  ASSERT(impl_ != NULL);
  return impl_->Type();
}

talk_base::Network* PortProxy::Network() const {
  ASSERT(impl_ != NULL);
  return impl_->Network();
}

void PortProxy::SetIceProtocolType(IceProtocolType protocol) {
  ASSERT(impl_ != NULL);
  impl_->SetIceProtocolType(protocol);
}

IceProtocolType PortProxy::IceProtocol() const {
  ASSERT(impl_ != NULL);
  return impl_->IceProtocol();
}

// Methods to set/get ICE role and tiebreaker values.
void PortProxy::SetIceRole(IceRole role) {
  ASSERT(impl_ != NULL);
  impl_->SetIceRole(role);
}

IceRole PortProxy::GetIceRole() const {
  ASSERT(impl_ != NULL);
  return impl_->GetIceRole();
}

void PortProxy::SetIceTiebreaker(uint64 tiebreaker) {
  ASSERT(impl_ != NULL);
  impl_->SetIceTiebreaker(tiebreaker);
}

uint64 PortProxy::IceTiebreaker() const {
  ASSERT(impl_ != NULL);
  return impl_->IceTiebreaker();
}

bool PortProxy::SharedSocket() const {
  ASSERT(impl_ != NULL);
  return impl_->SharedSocket();
}

void PortProxy::PrepareAddress() {
  ASSERT(impl_ != NULL);
  impl_->PrepareAddress();
}

Connection* PortProxy::CreateConnection(const Candidate& remote_candidate,
                                        CandidateOrigin origin) {
  ASSERT(impl_ != NULL);
  return impl_->CreateConnection(remote_candidate, origin);
}

int PortProxy::SendTo(const void* data,
                      size_t size,
                      const talk_base::SocketAddress& addr,
                      talk_base::DiffServCodePoint dscp,
                      bool payload) {
  ASSERT(impl_ != NULL);
  return impl_->SendTo(data, size, addr, dscp, payload);
}

int PortProxy::SetOption(talk_base::Socket::Option opt,
                         int value) {
  ASSERT(impl_ != NULL);
  return impl_->SetOption(opt, value);
}

int PortProxy::GetOption(talk_base::Socket::Option opt,
                         int* value) {
  ASSERT(impl_ != NULL);
  return impl_->GetOption(opt, value);
}

int PortProxy::GetError() {
  ASSERT(impl_ != NULL);
  return impl_->GetError();
}

const std::vector<Candidate>& PortProxy::Candidates() const {
  ASSERT(impl_ != NULL);
  return impl_->Candidates();
}

void PortProxy::SendBindingResponse(
    StunMessage* request, const talk_base::SocketAddress& addr) {
  ASSERT(impl_ != NULL);
  impl_->SendBindingResponse(request, addr);
}

Connection* PortProxy::GetConnection(
    const talk_base::SocketAddress& remote_addr) {
  ASSERT(impl_ != NULL);
  return impl_->GetConnection(remote_addr);
}

void PortProxy::SendBindingErrorResponse(
    StunMessage* request, const talk_base::SocketAddress& addr,
    int error_code, const std::string& reason) {
  ASSERT(impl_ != NULL);
  impl_->SendBindingErrorResponse(request, addr, error_code, reason);
}

void PortProxy::EnablePortPackets() {
  ASSERT(impl_ != NULL);
  impl_->EnablePortPackets();
}

std::string PortProxy::ToString() const {
  ASSERT(impl_ != NULL);
  return impl_->ToString();
}

void PortProxy::OnUnknownAddress(
    PortInterface *port,
    const talk_base::SocketAddress &addr,
    ProtocolType proto,
    IceMessage *stun_msg,
    const std::string &remote_username,
    bool port_muxed) {
  ASSERT(port == impl_);
  ASSERT(!port_muxed);
  SignalUnknownAddress(this, addr, proto, stun_msg, remote_username, true);
}

void PortProxy::OnRoleConflict(PortInterface* port) {
  ASSERT(port == impl_);
  SignalRoleConflict(this);
}

void PortProxy::OnPortDestroyed(PortInterface* port) {
  ASSERT(port == impl_);
  // |port| will be destroyed in PortAllocatorSessionMuxer.
  SignalDestroyed(this);
}

}  // namespace cricket
