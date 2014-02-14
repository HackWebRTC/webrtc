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

#include "talk/base/natsocketfactory.h"
#include "talk/base/natserver.h"
#include "talk/base/logging.h"

namespace talk_base {

RouteCmp::RouteCmp(NAT* nat) : symmetric(nat->IsSymmetric()) {
}

size_t RouteCmp::operator()(const SocketAddressPair& r) const {
  size_t h = r.source().Hash();
  if (symmetric)
    h ^= r.destination().Hash();
  return h;
}

bool RouteCmp::operator()(
      const SocketAddressPair& r1, const SocketAddressPair& r2) const {
  if (r1.source() < r2.source())
    return true;
  if (r2.source() < r1.source())
    return false;
  if (symmetric && (r1.destination() < r2.destination()))
    return true;
  if (symmetric && (r2.destination() < r1.destination()))
    return false;
  return false;
}

AddrCmp::AddrCmp(NAT* nat)
    : use_ip(nat->FiltersIP()), use_port(nat->FiltersPort()) {
}

size_t AddrCmp::operator()(const SocketAddress& a) const {
  size_t h = 0;
  if (use_ip)
    h ^= HashIP(a.ipaddr());
  if (use_port)
    h ^= a.port() | (a.port() << 16);
  return h;
}

bool AddrCmp::operator()(
      const SocketAddress& a1, const SocketAddress& a2) const {
  if (use_ip && (a1.ipaddr() < a2.ipaddr()))
    return true;
  if (use_ip && (a2.ipaddr() < a1.ipaddr()))
    return false;
  if (use_port && (a1.port() < a2.port()))
    return true;
  if (use_port && (a2.port() < a1.port()))
    return false;
  return false;
}

NATServer::NATServer(
    NATType type, SocketFactory* internal, const SocketAddress& internal_addr,
    SocketFactory* external, const SocketAddress& external_ip)
    : external_(external), external_ip_(external_ip.ipaddr(), 0) {
  nat_ = NAT::Create(type);

  server_socket_ = AsyncUDPSocket::Create(internal, internal_addr);
  server_socket_->SignalReadPacket.connect(this, &NATServer::OnInternalPacket);

  int_map_ = new InternalMap(RouteCmp(nat_));
  ext_map_ = new ExternalMap();
}

NATServer::~NATServer() {
  for (InternalMap::iterator iter = int_map_->begin();
       iter != int_map_->end();
       iter++)
    delete iter->second;

  delete nat_;
  delete server_socket_;
  delete int_map_;
  delete ext_map_;
}

void NATServer::OnInternalPacket(
    AsyncPacketSocket* socket, const char* buf, size_t size,
    const SocketAddress& addr, const PacketTime& packet_time) {

  // Read the intended destination from the wire.
  SocketAddress dest_addr;
  size_t length = UnpackAddressFromNAT(buf, size, &dest_addr);

  // Find the translation for these addresses (allocating one if necessary).
  SocketAddressPair route(addr, dest_addr);
  InternalMap::iterator iter = int_map_->find(route);
  if (iter == int_map_->end()) {
    Translate(route);
    iter = int_map_->find(route);
  }
  ASSERT(iter != int_map_->end());

  // Allow the destination to send packets back to the source.
  iter->second->WhitelistInsert(dest_addr);

  // Send the packet to its intended destination.
  talk_base::PacketOptions options;
  iter->second->socket->SendTo(buf + length, size - length, dest_addr, options);
}

void NATServer::OnExternalPacket(
    AsyncPacketSocket* socket, const char* buf, size_t size,
    const SocketAddress& remote_addr, const PacketTime& packet_time) {

  SocketAddress local_addr = socket->GetLocalAddress();

  // Find the translation for this addresses.
  ExternalMap::iterator iter = ext_map_->find(local_addr);
  ASSERT(iter != ext_map_->end());

  // Allow the NAT to reject this packet.
  if (ShouldFilterOut(iter->second, remote_addr)) {
    LOG(LS_INFO) << "Packet from " << remote_addr.ToSensitiveString()
                 << " was filtered out by the NAT.";
    return;
  }

  // Forward this packet to the internal address.
  // First prepend the address in a quasi-STUN format.
  scoped_ptr<char[]> real_buf(new char[size + kNATEncodedIPv6AddressSize]);
  size_t addrlength = PackAddressForNAT(real_buf.get(),
                                        size + kNATEncodedIPv6AddressSize,
                                        remote_addr);
  // Copy the data part after the address.
  talk_base::PacketOptions options;
  std::memcpy(real_buf.get() + addrlength, buf, size);
  server_socket_->SendTo(real_buf.get(), size + addrlength,
                         iter->second->route.source(), options);
}

void NATServer::Translate(const SocketAddressPair& route) {
  AsyncUDPSocket* socket = AsyncUDPSocket::Create(external_, external_ip_);

  if (!socket) {
    LOG(LS_ERROR) << "Couldn't find a free port!";
    return;
  }

  TransEntry* entry = new TransEntry(route, socket, nat_);
  (*int_map_)[route] = entry;
  (*ext_map_)[socket->GetLocalAddress()] = entry;
  socket->SignalReadPacket.connect(this, &NATServer::OnExternalPacket);
}

bool NATServer::ShouldFilterOut(TransEntry* entry,
                                const SocketAddress& ext_addr) {
  return entry->WhitelistContains(ext_addr);
}

NATServer::TransEntry::TransEntry(
    const SocketAddressPair& r, AsyncUDPSocket* s, NAT* nat)
    : route(r), socket(s) {
  whitelist = new AddressSet(AddrCmp(nat));
}

NATServer::TransEntry::~TransEntry() {
  delete whitelist;
  delete socket;
}

void NATServer::TransEntry::WhitelistInsert(const SocketAddress& addr) {
  CritScope cs(&crit_);
  whitelist->insert(addr);
}

bool NATServer::TransEntry::WhitelistContains(const SocketAddress& ext_addr) {
  CritScope cs(&crit_);
  return whitelist->find(ext_addr) == whitelist->end();
}

}  // namespace talk_base
