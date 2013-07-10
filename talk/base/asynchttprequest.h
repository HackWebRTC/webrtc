/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_BASE_ASYNCHTTPREQUEST_H_
#define TALK_BASE_ASYNCHTTPREQUEST_H_

#include <string>
#include "talk/base/event.h"
#include "talk/base/httpclient.h"
#include "talk/base/signalthread.h"
#include "talk/base/socketpool.h"
#include "talk/base/sslsocketfactory.h"

namespace talk_base {

class FirewallManager;

///////////////////////////////////////////////////////////////////////////////
// AsyncHttpRequest
// Performs an HTTP request on a background thread.  Notifies on the foreground
// thread once the request is done (successfully or unsuccessfully).
///////////////////////////////////////////////////////////////////////////////

class AsyncHttpRequest : public SignalThread {
 public:
  explicit AsyncHttpRequest(const std::string &user_agent);
  ~AsyncHttpRequest();

  // If start_delay is less than or equal to zero, this starts immediately.
  // Start_delay defaults to zero.
  int start_delay() const { return start_delay_; }
  void set_start_delay(int delay) { start_delay_ = delay; }

  const ProxyInfo& proxy() const { return proxy_; }
  void set_proxy(const ProxyInfo& proxy) {
    proxy_ = proxy;
  }
  void set_firewall(FirewallManager * firewall) {
    firewall_ = firewall;
  }

  // The DNS name of the host to connect to.
  const std::string& host() { return host_; }
  void set_host(const std::string& host) { host_ = host; }

  // The port to connect to on the target host.
  int port() { return port_; }
  void set_port(int port) { port_ = port; }

  // Whether the request should use SSL.
  bool secure() { return secure_; }
  void set_secure(bool secure) { secure_ = secure; }

  // Time to wait on the download, in ms.
  int timeout() { return timeout_; }
  void set_timeout(int timeout) { timeout_ = timeout; }

  // Fail redirects to allow analysis of redirect urls, etc.
  bool fail_redirect() const { return fail_redirect_; }
  void set_fail_redirect(bool redirect) { fail_redirect_ = redirect; }

  // Returns the redirect when redirection occurs
  const std::string& response_redirect() { return response_redirect_; }

  HttpRequestData& request() { return client_.request(); }
  HttpResponseData& response() { return client_.response(); }
  HttpErrorType error() { return error_; }

 protected:
  void set_error(HttpErrorType error) { error_ = error; }
  virtual void OnWorkStart();
  virtual void OnWorkStop();
  void OnComplete(HttpClient* client, HttpErrorType error);
  virtual void OnMessage(Message* message);
  virtual void DoWork();

 private:
  void LaunchRequest();

  int start_delay_;
  ProxyInfo proxy_;
  FirewallManager* firewall_;
  std::string host_;
  int port_;
  bool secure_;
  int timeout_;
  bool fail_redirect_;
  SslSocketFactory factory_;
  ReuseSocketPool pool_;
  HttpClient client_;
  HttpErrorType error_;
  std::string response_redirect_;
};

}  // namespace talk_base

#endif  // TALK_BASE_ASYNCHTTPREQUEST_H_
