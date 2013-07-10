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

#ifndef TALK_P2P_BASE_RAWTRANSPORT_H_
#define TALK_P2P_BASE_RAWTRANSPORT_H_

#include <string>
#include "talk/p2p/base/transport.h"

#if defined(FEATURE_ENABLE_PSTN)
namespace cricket {

// Implements a transport that only sends raw packets, no STUN.  As a result,
// it cannot do pings to determine connectivity, so it only uses a single port
// that it thinks will work.
class RawTransport : public Transport, public TransportParser {
 public:
  RawTransport(talk_base::Thread* signaling_thread,
               talk_base::Thread* worker_thread,
               const std::string& content_name,
               PortAllocator* allocator);
  virtual ~RawTransport();

  virtual bool ParseCandidates(SignalingProtocol protocol,
                               const buzz::XmlElement* elem,
                               const CandidateTranslator* translator,
                               Candidates* candidates,
                               ParseError* error);
  virtual bool WriteCandidates(SignalingProtocol protocol,
                               const Candidates& candidates,
                               const CandidateTranslator* translator,
                               XmlElements* candidate_elems,
                               WriteError* error);

 protected:
  // Creates and destroys raw channels.
  virtual TransportChannelImpl* CreateTransportChannel(int component);
  virtual void DestroyTransportChannel(TransportChannelImpl* channel);

 private:
  // Parses the given element, which should describe the address to use for a
  // given channel.  This will return false and signal an error if the address
  // or channel name is bad.
  bool ParseRawAddress(const buzz::XmlElement* elem,
                       talk_base::SocketAddress* addr,
                       ParseError* error);

  friend class RawTransportChannel;  // For ParseAddress.

  DISALLOW_EVIL_CONSTRUCTORS(RawTransport);
};

}  // namespace cricket

#endif  // defined(FEATURE_ENABLE_PSTN)

#endif  // TALK_P2P_BASE_RAWTRANSPORT_H_
