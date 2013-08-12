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

#ifndef TALK_P2P_BASE_PORTPROXY_H_
#define TALK_P2P_BASE_PORTPROXY_H_

#include "talk/base/sigslot.h"
#include "talk/p2p/base/portinterface.h"

namespace talk_base {
class Network;
}

namespace cricket {

class PortProxy : public PortInterface, public sigslot::has_slots<> {
 public:
  PortProxy() {}
  virtual ~PortProxy() {}

  PortInterface* impl() { return impl_; }
  void set_impl(PortInterface* port);

  virtual const std::string& Type() const;
  virtual talk_base::Network* Network() const;

  virtual void SetIceProtocolType(IceProtocolType protocol);
  virtual IceProtocolType IceProtocol() const;

  // Methods to set/get ICE role and tiebreaker values.
  virtual void SetIceRole(IceRole role);
  virtual IceRole GetIceRole() const;

  virtual void SetIceTiebreaker(uint64 tiebreaker);
  virtual uint64 IceTiebreaker() const;

  virtual bool SharedSocket() const;

  // Forwards call to the actual Port.
  virtual void PrepareAddress();
  virtual Connection* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin);
  virtual Connection* GetConnection(
      const talk_base::SocketAddress& remote_addr);

  virtual int SendTo(const void* data, size_t size,
                     const talk_base::SocketAddress& addr, bool payload);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetOption(talk_base::Socket::Option opt, int* value);
  virtual int GetError();

  virtual const std::vector<Candidate>& Candidates() const;

  virtual void SendBindingResponse(StunMessage* request,
                                   const talk_base::SocketAddress& addr);
  virtual void SendBindingErrorResponse(
        StunMessage* request, const talk_base::SocketAddress& addr,
        int error_code, const std::string& reason);

  virtual void EnablePortPackets();
  virtual std::string ToString() const;

 private:
  void OnUnknownAddress(PortInterface *port,
                        const talk_base::SocketAddress &addr,
                        ProtocolType proto,
                        IceMessage *stun_msg,
                        const std::string &remote_username,
                        bool port_muxed);
  void OnRoleConflict(PortInterface* port);
  void OnPortDestroyed(PortInterface* port);

  PortInterface* impl_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_PORTPROXY_H_
