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

#include "talk/base/base64.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/saslmechanism.h"

using talk_base::Base64;

namespace buzz {

XmlElement *
SaslMechanism::StartSaslAuth() {
  return new XmlElement(QN_SASL_AUTH, true);
}

XmlElement *
SaslMechanism::HandleSaslChallenge(const XmlElement * challenge) {
  return new XmlElement(QN_SASL_ABORT, true);
}

void
SaslMechanism::HandleSaslSuccess(const XmlElement * success) {
}

void
SaslMechanism::HandleSaslFailure(const XmlElement * failure) {
}

std::string
SaslMechanism::Base64Encode(const std::string & plain) {
  return Base64::Encode(plain);
}

std::string
SaslMechanism::Base64Decode(const std::string & encoded) {
  return Base64::Decode(encoded, Base64::DO_LAX);
}

std::string
SaslMechanism::Base64EncodeFromArray(const char * plain, size_t length) {
  std::string result;
  Base64::EncodeFromArray(plain, length, &result);
  return result;
}

}
