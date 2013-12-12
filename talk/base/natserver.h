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

#ifndef TALK_BASE_NATSERVER_H_
#define TALK_BASE_NATSERVER_H_

#include <map>
#include <set>

#include "talk/base/asyncudpsocket.h"
#include "talk/base/socketaddresspair.h"
#include "talk/base/thread.h"
#include "talk/base/socketfactory.h"
#include "talk/base/nattypes.h"

namespace talk_base {

// Change how routes (socketaddress pairs) are compared based on the type of
// NAT.  The NAT server maintains a hashtable of the routes that it knows
// about.  So these affect which routes are treated the same.
struct RouteCmp {
  explicit RouteCmp(NAT* nat);
  size_t operator()(const SocketAddressPair& r) const;
  bool operator()(
      const SocketAddressPair& r1, const SocketAddressPair& r2) const;

  bool symmetric;
};

// Changes how addresses are compared based on the filtering rules of the NAT.
struct AddrCmp {
  explicit AddrCmp(NAT* nat);
  size_t operator()(const SocketAddress& r) const;
  bool operator()(const SocketAddress& r1, const SocketAddress& r2) const;

  bool use_ip;
  bool use_port;
};

// Implements the NAT device.  It listens for packets on the internal network,
// translates them, and sends them out over the external network.

const int NAT_SERVER_PORT = 4237;

class NATServer : public sigslot::has_slots<> {
 public:
  NATServer(
      NATType type, SocketFactory* internal, const SocketAddress& internal_addr,
      SocketFactory* external, const SocketAddress& external_ip);
  ~NATServer();

  SocketAddress internal_address() const {
    return server_socket_->GetLocalAddress();
  }

  // Packets received on one of the networks.
  void OnInternalPacket(AsyncPacketSocket* socket, const char* buf,
                        size_t size, const SocketAddress& addr,
                        const PacketTime& packet_time);
  void OnExternalPacket(AsyncPacketSocket* socket, const char* buf,
                        size_t size, const SocketAddress& remote_addr,
                        const PacketTime& packet_time);

 private:
  typedef std::set<SocketAddress, AddrCmp> AddressSet;

  /* Records a translation and the associated external socket. */
  struct TransEntry {
    TransEntry(const SocketAddressPair& r, AsyncUDPSocket* s, NAT* nat);
    ~TransEntry();

    void WhitelistInsert(const SocketAddress& addr);
    bool WhitelistContains(const SocketAddress& ext_addr);

    SocketAddressPair route;
    AsyncUDPSocket* socket;
    AddressSet* whitelist;
    CriticalSection crit_;
  };

  typedef std::map<SocketAddressPair, TransEntry*, RouteCmp> InternalMap;
  typedef std::map<SocketAddress, TransEntry*> ExternalMap;

  /* Creates a new entry that translates the given route. */
  void Translate(const SocketAddressPair& route);

  /* Determines whether the NAT would filter out a packet from this address. */
  bool ShouldFilterOut(TransEntry* entry, const SocketAddress& ext_addr);

  NAT* nat_;
  SocketFactory* internal_;
  SocketFactory* external_;
  SocketAddress external_ip_;
  AsyncUDPSocket* server_socket_;
  AsyncSocket* tcp_server_socket_;
  InternalMap* int_map_;
  ExternalMap* ext_map_;
  DISALLOW_EVIL_CONSTRUCTORS(NATServer);
};

}  // namespace talk_base

#endif  // TALK_BASE_NATSERVER_H_
