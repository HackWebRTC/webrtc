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

#ifndef TALK_P2P_BASE_TRANSPORTCHANNELIMPL_H_
#define TALK_P2P_BASE_TRANSPORTCHANNELIMPL_H_

#include <string>
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannel.h"

namespace buzz { class XmlElement; }

namespace cricket {

class Candidate;

// Base class for real implementations of TransportChannel.  This includes some
// methods called only by Transport, which do not need to be exposed to the
// client.
class TransportChannelImpl : public TransportChannel {
 public:
  explicit TransportChannelImpl(const std::string& content_name, int component)
      : TransportChannel(content_name, component) {}

  // Returns the transport that created this channel.
  virtual Transport* GetTransport() = 0;

  // For ICE channels.
  virtual IceRole GetIceRole() const = 0;
  virtual void SetIceRole(IceRole role) = 0;
  virtual void SetIceTiebreaker(uint64 tiebreaker) = 0;
  // To toggle G-ICE/ICE.
  virtual void SetIceProtocolType(IceProtocolType type) = 0;
  // SetIceCredentials only need to be implemented by the ICE
  // transport channels. Non-ICE transport channels can just ignore.
  // The ufrag and pwd should be set before the Connect() is called.
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd)  = 0;
  // SetRemoteIceCredentials only need to be implemented by the ICE
  // transport channels. Non-ICE transport channels can just ignore.
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd) = 0;

  // SetRemoteIceMode must be implemented only by the ICE transport channels.
  virtual void SetRemoteIceMode(IceMode mode) = 0;

  // Begins the process of attempting to make a connection to the other client.
  virtual void Connect() = 0;

  // Resets this channel back to the initial state (i.e., not connecting).
  virtual void Reset() = 0;

  // Allows an individual channel to request signaling and be notified when it
  // is ready.  This is useful if the individual named channels have need to
  // send their own transport-info stanzas.
  sigslot::signal1<TransportChannelImpl*> SignalRequestSignaling;
  virtual void OnSignalingReady() = 0;

  // Handles sending and receiving of candidates.  The Transport
  // receives the candidates and may forward them to the relevant
  // channel.
  //
  // Note: Since candidates are delivered asynchronously to the
  // channel, they cannot return an error if the message is invalid.
  // It is assumed that the Transport will have checked validity
  // before forwarding.
  sigslot::signal2<TransportChannelImpl*,
                   const Candidate&> SignalCandidateReady;
  virtual void OnCandidate(const Candidate& candidate) = 0;

  // DTLS methods
  // Set DTLS local identity.  The identity object is not copied, but the caller
  // retains ownership and must delete it after this TransportChannelImpl is
  // destroyed.
  // TODO(bemasc): Fix the ownership semantics of this method.
  virtual bool SetLocalIdentity(talk_base::SSLIdentity* identity) = 0;

  // Set DTLS Remote fingerprint. Must be after local identity set.
  virtual bool SetRemoteFingerprint(const std::string& digest_alg,
    const uint8* digest,
    size_t digest_len) = 0;

  virtual bool SetSslRole(talk_base::SSLRole role) = 0;

  // TransportChannel is forwarding this signal from PortAllocatorSession.
  sigslot::signal1<TransportChannelImpl*> SignalCandidatesAllocationDone;

  // Invoked when there is conflict in the ICE role between local and remote
  // agents.
  sigslot::signal1<TransportChannelImpl*> SignalRoleConflict;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TransportChannelImpl);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTCHANNELIMPL_H_
