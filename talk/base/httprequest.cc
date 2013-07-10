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

#include "talk/base/httprequest.h"

#include "talk/base/common.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/httpclient.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/socketadapters.h"
#include "talk/base/socketpool.h"
#include "talk/base/ssladapter.h"

using namespace talk_base;

///////////////////////////////////////////////////////////////////////////////
// HttpMonitor
///////////////////////////////////////////////////////////////////////////////

HttpMonitor::HttpMonitor(SocketServer *ss) {
  ASSERT(Thread::Current() != NULL);
  ss_ = ss;
  reset();
}

void HttpMonitor::Connect(HttpClient *http) {
  http->SignalHttpClientComplete.connect(this,
    &HttpMonitor::OnHttpClientComplete);
}

void HttpMonitor::OnHttpClientComplete(HttpClient * http, HttpErrorType error) {
  complete_ = true;
  error_ = error;
  ss_->WakeUp();
}

///////////////////////////////////////////////////////////////////////////////
// HttpRequest
///////////////////////////////////////////////////////////////////////////////

const int kDefaultHTTPTimeout = 30 * 1000; // 30 sec

HttpRequest::HttpRequest(const std::string &user_agent)
    : firewall_(0), port_(80), secure_(false),
      timeout_(kDefaultHTTPTimeout), fail_redirect_(false),
      client_(user_agent.c_str(), NULL), error_(HE_NONE) {
}

void HttpRequest::Send() {
  // TODO: Rewrite this to use the thread's native socket server, and a more
  // natural flow?

  PhysicalSocketServer physical;
  SocketServer * ss = &physical;
  if (firewall_) {
    ss = new FirewallSocketServer(ss, firewall_);
  }

  SslSocketFactory factory(ss, client_.agent());
  factory.SetProxy(proxy_);
  if (secure_)
    factory.UseSSL(host_.c_str());

  //factory.SetLogging("HttpRequest");

  ReuseSocketPool pool(&factory);
  client_.set_pool(&pool);

  bool transparent_proxy = (port_ == 80) && ((proxy_.type == PROXY_HTTPS) ||
                           (proxy_.type == PROXY_UNKNOWN));

  if (transparent_proxy) {
    client_.set_proxy(proxy_);
  }
  client_.set_fail_redirect(fail_redirect_);

  SocketAddress server(host_, port_);
  client_.set_server(server);

  LOG(LS_INFO) << "HttpRequest start: " << host_ + client_.request().path;

  HttpMonitor monitor(ss);
  monitor.Connect(&client_);
  client_.start();
  ss->Wait(timeout_, true);
  if (!monitor.done()) {
    LOG(LS_INFO) << "HttpRequest request timed out";
    client_.reset();
    return;
  }

  set_error(monitor.error());
  if (error_) {
    LOG(LS_INFO) << "HttpRequest request error: " << error_;
    return;
  }

  std::string value;
  if (client_.response().hasHeader(HH_LOCATION, &value)) {
    response_redirect_ = value.c_str();
  }
}
