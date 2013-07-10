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

#ifndef TALK_P2P_BASE_P2PTRANSPORT_H_
#define TALK_P2P_BASE_P2PTRANSPORT_H_

#include <string>
#include <vector>
#include "talk/p2p/base/transport.h"

namespace cricket {

class P2PTransport : public Transport {
 public:
  P2PTransport(talk_base::Thread* signaling_thread,
               talk_base::Thread* worker_thread,
               const std::string& content_name,
               PortAllocator* allocator);
  virtual ~P2PTransport();

 protected:
  // Creates and destroys P2PTransportChannel.
  virtual TransportChannelImpl* CreateTransportChannel(int component);
  virtual void DestroyTransportChannel(TransportChannelImpl* channel);

  friend class P2PTransportChannel;

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransport);
};

class P2PTransportParser : public TransportParser {
 public:
  P2PTransportParser() {}
  // Translator may be null, in which case ParseCandidates should
  // return false if there are candidates to parse.  We can't not call
  // ParseCandidates because there's no way to know ahead of time if
  // there are candidates or not.

  // Jingle-specific functions; can be used with either ICE, GICE, or HYBRID.
  virtual bool ParseTransportDescription(const buzz::XmlElement* elem,
                                         const CandidateTranslator* translator,
                                         TransportDescription* desc,
                                         ParseError* error);
  virtual bool WriteTransportDescription(const TransportDescription& desc,
                                         const CandidateTranslator* translator,
                                         buzz::XmlElement** elem,
                                         WriteError* error);

  // Legacy Gingle functions; only can be used with GICE.
  virtual bool ParseGingleCandidate(const buzz::XmlElement* elem,
                                    const CandidateTranslator* translator,
                                    Candidate* candidate,
                                    ParseError* error);
  virtual bool WriteGingleCandidate(const Candidate& candidate,
                                    const CandidateTranslator* translator,
                                    buzz::XmlElement** elem,
                                    WriteError* error);

 private:
  bool ParseCandidate(TransportProtocol proto,
                      const buzz::XmlElement* elem,
                      const CandidateTranslator* translator,
                      Candidate* candidate,
                      ParseError* error);
  bool WriteCandidate(TransportProtocol proto,
                      const Candidate& candidate,
                      const CandidateTranslator* translator,
                      buzz::XmlElement* elem,
                      WriteError* error);
  bool VerifyUsernameFormat(TransportProtocol proto,
                            const std::string& username,
                            ParseError* error);

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransportParser);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_P2PTRANSPORT_H_
