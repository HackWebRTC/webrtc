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

#ifndef TALK_XMPP_SASLCOOKIEMECHANISM_H_
#define TALK_XMPP_SASLCOOKIEMECHANISM_H_

#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/saslmechanism.h"
#include "talk/xmpp/constants.h"

namespace buzz {

class SaslCookieMechanism : public SaslMechanism {

public:
  SaslCookieMechanism(const std::string & mechanism,
                      const std::string & username,
                      const std::string & cookie,
                      const std::string & token_service)
    : mechanism_(mechanism),
      username_(username),
      cookie_(cookie),
      token_service_(token_service) {}

  SaslCookieMechanism(const std::string & mechanism,
                      const std::string & username,
                      const std::string & cookie)
    : mechanism_(mechanism),
      username_(username),
      cookie_(cookie),
      token_service_("") {}

  virtual std::string GetMechanismName() { return mechanism_; }

  virtual XmlElement * StartSaslAuth() {
    // send initial request
    XmlElement * el = new XmlElement(QN_SASL_AUTH, true);
    el->AddAttr(QN_MECHANISM, mechanism_);
    if (!token_service_.empty()) {
      el->AddAttr(QN_GOOGLE_AUTH_SERVICE, token_service_);
    }

    std::string credential;
    credential.append("\0", 1);
    credential.append(username_);
    credential.append("\0", 1);
    credential.append(cookie_);
    el->AddText(Base64Encode(credential));
    return el;
  }

private:
  std::string mechanism_;
  std::string username_;
  std::string cookie_;
  std::string token_service_;
};

}

#endif  // TALK_XMPP_SASLCOOKIEMECHANISM_H_
