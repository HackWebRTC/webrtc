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

#ifndef TALK_XMPP_XMPPAUTH_H_
#define TALK_XMPP_XMPPAUTH_H_

#include <vector>

#include "talk/base/cryptstring.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/saslhandler.h"
#include "talk/xmpp/prexmppauth.h"

class XmppAuth: public buzz::PreXmppAuth {
public:
  XmppAuth();
  virtual ~XmppAuth();

  // TODO: Just have one "secret" that is either pass or
  // token?
  virtual void StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string& auth_mechanism,
                                const std::string& auth_token);

  virtual bool IsAuthDone() const { return done_; }
  virtual bool IsAuthorized() const { return true; }
  virtual bool HadError() const { return false; }
  virtual int  GetError() const { return 0; }
  virtual buzz::CaptchaChallenge GetCaptchaChallenge() const {
      return buzz::CaptchaChallenge();
  }
  virtual std::string GetAuthMechanism() const { return auth_mechanism_; }
  virtual std::string GetAuthToken() const { return auth_token_; }

  virtual std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms,
      bool encrypted);

  virtual buzz::SaslMechanism * CreateSaslMechanism(
      const std::string& mechanism);

private:
  buzz::Jid jid_;
  talk_base::CryptString passwd_;
  std::string auth_mechanism_;
  std::string auth_token_;
  bool done_;
};

#endif  // TALK_XMPP_XMPPAUTH_H_

