/*
 * libjingle
 * Copyright 2012 Google Inc. All rights reserved.
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

#ifndef TALK_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_
#define TALK_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_

#include "talk/p2p/base/transportdescription.h"

namespace talk_base {
class SSLIdentity;
}

namespace cricket {

struct TransportOptions {
  TransportOptions() : ice_restart(false), prefer_passive_role(false) {}
  bool ice_restart;
  bool prefer_passive_role;
};

// Creates transport descriptions according to the supplied configuration.
// When creating answers, performs the appropriate negotiation
// of the various fields to determine the proper result.
class TransportDescriptionFactory {
 public:
  // Default ctor; use methods below to set configuration.
  TransportDescriptionFactory();
  SecurePolicy secure() const { return secure_; }
  // The identity to use when setting up DTLS.
  talk_base::SSLIdentity* identity() const { return identity_; }

  // Specifies the transport protocol to be use.
  void set_protocol(TransportProtocol protocol) { protocol_ = protocol; }
  // Specifies the transport security policy to use.
  void set_secure(SecurePolicy s) { secure_ = s; }
  // Specifies the identity to use (only used when secure is not SEC_DISABLED).
  void set_identity(talk_base::SSLIdentity* identity) { identity_ = identity; }
  // Specifies the algorithm to use when creating an identity digest.
  void set_digest_algorithm(const std::string& alg) { digest_alg_ = alg; }

  // Creates a transport description suitable for use in an offer.
  TransportDescription* CreateOffer(const TransportOptions& options,
      const TransportDescription* current_description) const;
  // Create a transport description that is a response to an offer.
  TransportDescription* CreateAnswer(
      const TransportDescription* offer,
      const TransportOptions& options,
      const TransportDescription* current_description) const;

 private:
  bool SetSecurityInfo(TransportDescription* description,
                       ConnectionRole role) const;

  TransportProtocol protocol_;
  SecurePolicy secure_;
  talk_base::SSLIdentity* identity_;
  std::string digest_alg_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_
