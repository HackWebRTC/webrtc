/*
 * libjingle
 * Copyright 2006, Google Inc.
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

#ifndef _HTTPREQUEST_H_
#define _HTTPREQUEST_H_

#include "talk/base/httpclient.h"
#include "talk/base/logging.h"
#include "talk/base/proxyinfo.h"
#include "talk/base/socketserver.h"
#include "talk/base/thread.h"
#include "talk/base/sslsocketfactory.h"  // Deprecated include

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// HttpRequest
///////////////////////////////////////////////////////////////////////////////

class FirewallManager;
class MemoryStream;

class HttpRequest {
public:
  HttpRequest(const std::string &user_agent);

  void Send();

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

  // Returns the redirect when redirection occurs
  const std::string& response_redirect() { return response_redirect_; }

  // Time to wait on the download, in ms.  Default is 5000 (5s)
  int timeout() { return timeout_; }
  void set_timeout(int timeout) { timeout_ = timeout; }

  // Fail redirects to allow analysis of redirect urls, etc.
  bool fail_redirect() const { return fail_redirect_; }
  void set_fail_redirect(bool fail_redirect) { fail_redirect_ = fail_redirect; }

  HttpRequestData& request() { return client_.request(); }
  HttpResponseData& response() { return client_.response(); }
  HttpErrorType error() { return error_; }

protected:
  void set_error(HttpErrorType error) { error_ = error; }

private:
  ProxyInfo proxy_;
  FirewallManager * firewall_;
  std::string host_;
  int port_;
  bool secure_;
  int timeout_;
  bool fail_redirect_;
  HttpClient client_;
  HttpErrorType error_;
  std::string response_redirect_;
};

///////////////////////////////////////////////////////////////////////////////
// HttpMonitor
///////////////////////////////////////////////////////////////////////////////

class HttpMonitor : public sigslot::has_slots<> {
public:
  HttpMonitor(SocketServer *ss);

  void reset() {
    complete_ = false;
    error_ = HE_DEFAULT;
  }

  bool done() const { return complete_; }
  HttpErrorType error() const { return error_; }

  void Connect(HttpClient* http);
  void OnHttpClientComplete(HttpClient * http, HttpErrorType error);

private:
  bool complete_;
  HttpErrorType error_;
  SocketServer *ss_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base_

#endif  // _HTTPREQUEST_H_
