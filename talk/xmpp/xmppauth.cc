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

#include "talk/xmpp/xmppauth.h"

#include <algorithm>

#include "talk/xmpp/constants.h"
#include "talk/xmpp/saslcookiemechanism.h"
#include "talk/xmpp/saslplainmechanism.h"

XmppAuth::XmppAuth() : done_(false) {
}

XmppAuth::~XmppAuth() {
}

void XmppAuth::StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string& auth_mechanism,
                                const std::string& auth_token) {
  jid_ = jid;
  passwd_ = pass;
  auth_mechanism_ = auth_mechanism;
  auth_token_ = auth_token;
  done_ = true;

  SignalAuthDone();
}

static bool contains(const std::vector<std::string>& strings,
                     const std::string& string) {
  return std::find(strings.begin(), strings.end(), string) != strings.end();
}

std::string XmppAuth::ChooseBestSaslMechanism(
    const std::vector<std::string>& mechanisms,
    bool encrypted) {
  // First try Oauth2.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_OAUTH2 &&
      contains(mechanisms, buzz::AUTH_MECHANISM_OAUTH2)) {
    return buzz::AUTH_MECHANISM_OAUTH2;
  }

  // A token is the weakest auth - 15s, service-limited, so prefer it.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_GOOGLE_TOKEN &&
      contains(mechanisms, buzz::AUTH_MECHANISM_GOOGLE_TOKEN)) {
    return buzz::AUTH_MECHANISM_GOOGLE_TOKEN;
  }

  // A cookie is the next weakest - 14 days.
  if (GetAuthMechanism() == buzz::AUTH_MECHANISM_GOOGLE_COOKIE &&
      contains(mechanisms, buzz::AUTH_MECHANISM_GOOGLE_COOKIE)) {
    return buzz::AUTH_MECHANISM_GOOGLE_COOKIE;
  }

  // As a last resort, use plain authentication.
  if (contains(mechanisms, buzz::AUTH_MECHANISM_PLAIN)) {
    return buzz::AUTH_MECHANISM_PLAIN;
  }

  // No good mechanism found
  return "";
}

buzz::SaslMechanism* XmppAuth::CreateSaslMechanism(
    const std::string& mechanism) {
  if (mechanism == buzz::AUTH_MECHANISM_OAUTH2) {
    return new buzz::SaslCookieMechanism(
        mechanism, jid_.Str(), auth_token_, "oauth2");
  } else if (mechanism == buzz::AUTH_MECHANISM_GOOGLE_TOKEN) {
    return new buzz::SaslCookieMechanism(mechanism, jid_.Str(), auth_token_);
  // } else if (mechanism == buzz::AUTH_MECHANISM_GOOGLE_COOKIE) {
  //   return new buzz::SaslCookieMechanism(mechanism, jid.Str(), sid_);
  } else if (mechanism == buzz::AUTH_MECHANISM_PLAIN) {
    return new buzz::SaslPlainMechanism(jid_, passwd_);
  } else {
    return NULL;
  }
}
