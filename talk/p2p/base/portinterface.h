/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_P2P_BASE_PORTINTERFACE_H_
#define TALK_P2P_BASE_PORTINTERFACE_H_

#include <string>

#include "talk/base/socketaddress.h"
#include "talk/p2p/base/transport.h"

namespace talk_base {
class Network;
class PacketSocketFactory;
}

namespace cricket {
class Connection;
class IceMessage;
class StunMessage;

enum ProtocolType {
  PROTO_UDP,
  PROTO_TCP,
  PROTO_SSLTCP,
  PROTO_LAST = PROTO_SSLTCP
};

// Defines the interface for a port, which represents a local communication
// mechanism that can be used to create connections to similar mechanisms of
// the other client. Various types of ports will implement this interface.
class PortInterface {
 public:
  virtual ~PortInterface() {}

  virtual const std::string& Type() const = 0;
  virtual talk_base::Network* Network() const = 0;

  virtual void SetIceProtocolType(IceProtocolType protocol) = 0;
  virtual IceProtocolType IceProtocol() const = 0;

  // Methods to set/get ICE role and tiebreaker values.
  virtual void SetIceRole(IceRole role) = 0;
  virtual IceRole GetIceRole() const = 0;

  virtual void SetIceTiebreaker(uint64 tiebreaker) = 0;
  virtual uint64 IceTiebreaker() const = 0;

  virtual bool SharedSocket() const = 0;

  // PrepareAddress will attempt to get an address for this port that other
  // clients can send to.  It may take some time before the address is ready.
  // Once it is ready, we will send SignalAddressReady.  If errors are
  // preventing the port from getting an address, it may send
  // SignalAddressError.
  virtual void PrepareAddress() = 0;

  // Returns the connection to the given address or NULL if none exists.
  virtual Connection* GetConnection(
      const talk_base::SocketAddress& remote_addr) = 0;

  // Creates a new connection to the given address.
  enum CandidateOrigin { ORIGIN_THIS_PORT, ORIGIN_OTHER_PORT, ORIGIN_MESSAGE };
  virtual Connection* CreateConnection(
      const Candidate& remote_candidate, CandidateOrigin origin) = 0;

  // Functions on the underlying socket(s).
  virtual int SetOption(talk_base::Socket::Option opt, int value) = 0;
  virtual int GetError() = 0;

  virtual int GetOption(talk_base::Socket::Option opt, int* value) = 0;

  virtual const std::vector<Candidate>& Candidates() const = 0;

  // Sends the given packet to the given address, provided that the address is
  // that of a connection or an address that has sent to us already.
  virtual int SendTo(const void* data, size_t size,
                     const talk_base::SocketAddress& addr, bool payload) = 0;

  // Indicates that we received a successful STUN binding request from an
  // address that doesn't correspond to any current connection.  To turn this
  // into a real connection, call CreateConnection.
  sigslot::signal6<PortInterface*, const talk_base::SocketAddress&,
                   ProtocolType, IceMessage*, const std::string&,
                   bool> SignalUnknownAddress;

  // Sends a response message (normal or error) to the given request.  One of
  // these methods should be called as a response to SignalUnknownAddress.
  // NOTE: You MUST call CreateConnection BEFORE SendBindingResponse.
  virtual void SendBindingResponse(StunMessage* request,
                                   const talk_base::SocketAddress& addr) = 0;
  virtual void SendBindingErrorResponse(
      StunMessage* request, const talk_base::SocketAddress& addr,
      int error_code, const std::string& reason) = 0;

  // Signaled when this port decides to delete itself because it no longer has
  // any usefulness.
  sigslot::signal1<PortInterface*> SignalDestroyed;

  // Signaled when Port discovers ice role conflict with the peer.
  sigslot::signal1<PortInterface*> SignalRoleConflict;

  // Normally, packets arrive through a connection (or they result signaling of
  // unknown address).  Calling this method turns off delivery of packets
  // through their respective connection and instead delivers every packet
  // through this port.
  virtual void EnablePortPackets() = 0;
  sigslot::signal4<PortInterface*, const char*, size_t,
                   const talk_base::SocketAddress&> SignalReadPacket;

  virtual std::string ToString() const = 0;

 protected:
  PortInterface() {}
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_PORTINTERFACE_H_
