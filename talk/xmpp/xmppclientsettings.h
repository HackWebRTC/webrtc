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

#ifndef TALK_XMPP_XMPPCLIENTSETTINGS_H_
#define TALK_XMPP_XMPPCLIENTSETTINGS_H_

#include "talk/p2p/base/port.h"
#include "talk/base/cryptstring.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {

class XmppUserSettings {
 public:
  XmppUserSettings()
    : use_tls_(buzz::TLS_DISABLED),
      allow_plain_(false) {
  }

  void set_user(const std::string& user) { user_ = user; }
  void set_host(const std::string& host) { host_ = host; }
  void set_pass(const talk_base::CryptString& pass) { pass_ = pass; }
  void set_auth_token(const std::string& mechanism,
                      const std::string& token) {
    auth_mechanism_ = mechanism;
    auth_token_ = token;
  }
  void set_resource(const std::string& resource) { resource_ = resource; }
  void set_use_tls(const TlsOptions use_tls) { use_tls_ = use_tls; }
  void set_allow_plain(bool f) { allow_plain_ = f; }
  void set_test_server_domain(const std::string& test_server_domain) {
    test_server_domain_ = test_server_domain;
  }
  void set_token_service(const std::string& token_service) {
    token_service_ = token_service;
  }

  const std::string& user() const { return user_; }
  const std::string& host() const { return host_; }
  const talk_base::CryptString& pass() const { return pass_; }
  const std::string& auth_mechanism() const { return auth_mechanism_; }
  const std::string& auth_token() const { return auth_token_; }
  const std::string& resource() const { return resource_; }
  TlsOptions use_tls() const { return use_tls_; }
  bool allow_plain() const { return allow_plain_; }
  const std::string& test_server_domain() const { return test_server_domain_; }
  const std::string& token_service() const { return token_service_; }

 private:
  std::string user_;
  std::string host_;
  talk_base::CryptString pass_;
  std::string auth_mechanism_;
  std::string auth_token_;
  std::string resource_;
  TlsOptions use_tls_;
  bool allow_plain_;
  std::string test_server_domain_;
  std::string token_service_;
};

class XmppClientSettings : public XmppUserSettings {
 public:
  XmppClientSettings()
    : protocol_(cricket::PROTO_TCP),
      proxy_(talk_base::PROXY_NONE),
      proxy_port_(80),
      use_proxy_auth_(false) {
  }

  void set_server(const talk_base::SocketAddress& server) {
      server_ = server;
  }
  void set_protocol(cricket::ProtocolType protocol) { protocol_ = protocol; }
  void set_proxy(talk_base::ProxyType f) { proxy_ = f; }
  void set_proxy_host(const std::string& host) { proxy_host_ = host; }
  void set_proxy_port(int port) { proxy_port_ = port; };
  void set_use_proxy_auth(bool f) { use_proxy_auth_ = f; }
  void set_proxy_user(const std::string& user) { proxy_user_ = user; }
  void set_proxy_pass(const talk_base::CryptString& pass) { proxy_pass_ = pass; }

  const talk_base::SocketAddress& server() const { return server_; }
  cricket::ProtocolType protocol() const { return protocol_; }
  talk_base::ProxyType proxy() const { return proxy_; }
  const std::string& proxy_host() const { return proxy_host_; }
  int proxy_port() const { return proxy_port_; }
  bool use_proxy_auth() const { return use_proxy_auth_; }
  const std::string& proxy_user() const { return proxy_user_; }
  const talk_base::CryptString& proxy_pass() const { return proxy_pass_; }

 private:
  talk_base::SocketAddress server_;
  cricket::ProtocolType protocol_;
  talk_base::ProxyType proxy_;
  std::string proxy_host_;
  int proxy_port_;
  bool use_proxy_auth_;
  std::string proxy_user_;
  talk_base::CryptString proxy_pass_;
};

}

#endif  // TALK_XMPP_XMPPCLIENT_H_
