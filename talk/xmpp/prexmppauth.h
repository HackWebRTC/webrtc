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

#ifndef TALK_XMPP_PREXMPPAUTH_H_
#define TALK_XMPP_PREXMPPAUTH_H_

#include "talk/base/cryptstring.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/saslhandler.h"

namespace talk_base {
  class SocketAddress;
}

namespace buzz {

class Jid;
class SaslMechanism;

class CaptchaChallenge {
 public:
  CaptchaChallenge() : captcha_needed_(false) {}
  CaptchaChallenge(const std::string& token, const std::string& url)
    : captcha_needed_(true), captcha_token_(token), captcha_image_url_(url) {
  }

  bool captcha_needed() const { return captcha_needed_; }
  const std::string& captcha_token() const { return captcha_token_; }

  // This url is relative to the gaia server.  Once we have better tools
  // for cracking URLs, we should probably make this a full URL
  const std::string& captcha_image_url() const { return captcha_image_url_; }

 private:
  bool captcha_needed_;
  std::string captcha_token_;
  std::string captcha_image_url_;
};

class PreXmppAuth : public SaslHandler {
public:
  virtual ~PreXmppAuth() {}

  virtual void StartPreXmppAuth(
    const Jid& jid,
    const talk_base::SocketAddress& server,
    const talk_base::CryptString& pass,
    const std::string& auth_mechanism,
    const std::string& auth_token) = 0;

  sigslot::signal0<> SignalAuthDone;

  virtual bool IsAuthDone() const = 0;
  virtual bool IsAuthorized() const = 0;
  virtual bool HadError() const = 0;
  virtual int GetError() const = 0;
  virtual CaptchaChallenge GetCaptchaChallenge() const = 0;
  virtual std::string GetAuthMechanism() const = 0;
  virtual std::string GetAuthToken() const = 0;
};

}

#endif  // TALK_XMPP_PREXMPPAUTH_H_
