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

#ifndef _SASLMECHANISM_H_
#define _SASLMECHANISM_H_

#include <string>

namespace buzz {

class XmlElement;


// Defines a mechnanism to do SASL authentication.
// Subclass instances should have a self-contained way to present
// credentials.
class SaslMechanism {

public:
  
  // Intended to be subclassed
  virtual ~SaslMechanism() {}

  // Should return the name of the SASL mechanism, e.g., "PLAIN"
  virtual std::string GetMechanismName() = 0;

  // Should generate the initial "auth" request.  Default is just <auth/>.
  virtual XmlElement * StartSaslAuth();

  // Should respond to a SASL "<challenge>" request.  Default is
  // to abort (for mechanisms that do not do challenge-response)
  virtual XmlElement * HandleSaslChallenge(const XmlElement * challenge);

  // Notification of a SASL "<success>".  Sometimes information
  // is passed on success.
  virtual void HandleSaslSuccess(const XmlElement * success);

  // Notification of a SASL "<failure>".  Sometimes information
  // for the user is passed on failure.
  virtual void HandleSaslFailure(const XmlElement * failure);

protected:
  static std::string Base64Encode(const std::string & plain);
  static std::string Base64Decode(const std::string & encoded);
  static std::string Base64EncodeFromArray(const char * plain, size_t length);
};

}

#endif
