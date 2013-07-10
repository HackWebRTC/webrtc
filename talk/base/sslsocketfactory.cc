/*
 * libjingle
 * Copyright 2007, Google Inc.
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

#include "talk/base/autodetectproxy.h"
#include "talk/base/httpcommon.h"
#include "talk/base/httpcommon-inl.h"
#include "talk/base/socketadapters.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslsocketfactory.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// ProxySocketAdapter
// TODO: Consider combining AutoDetectProxy and ProxySocketAdapter.  I think
// the socket adapter is the more appropriate idiom for automatic proxy
// detection.  We may or may not want to combine proxydetect.* as well.
///////////////////////////////////////////////////////////////////////////////

class ProxySocketAdapter : public AsyncSocketAdapter {
 public:
  ProxySocketAdapter(SslSocketFactory* factory, int family, int type)
      : AsyncSocketAdapter(NULL), factory_(factory), family_(family),
        type_(type), detect_(NULL) {
  }
  virtual ~ProxySocketAdapter() {
    Close();
  }

  virtual int Connect(const SocketAddress& addr) {
    ASSERT(NULL == detect_);
    ASSERT(NULL == socket_);
    remote_ = addr;
    if (remote_.IsAnyIP() && remote_.hostname().empty()) {
      LOG_F(LS_ERROR) << "Empty address";
      return SOCKET_ERROR;
    }
    Url<char> url("/", remote_.HostAsURIString(), remote_.port());
    detect_ = new AutoDetectProxy(factory_->agent_);
    detect_->set_server_url(url.url());
    detect_->SignalWorkDone.connect(this,
        &ProxySocketAdapter::OnProxyDetectionComplete);
    detect_->Start();
    return SOCKET_ERROR;
  }
  virtual int GetError() const {
    if (socket_) {
      return socket_->GetError();
    }
    return detect_ ? EWOULDBLOCK : EADDRNOTAVAIL;
  }
  virtual int Close() {
    if (socket_) {
      return socket_->Close();
    }
    if (detect_) {
      detect_->Destroy(false);
      detect_ = NULL;
    }
    return 0;
  }
  virtual ConnState GetState() const {
    if (socket_) {
      return socket_->GetState();
    }
    return detect_ ? CS_CONNECTING : CS_CLOSED;
  }

private:
  // AutoDetectProxy Slots
  void OnProxyDetectionComplete(SignalThread* thread) {
    ASSERT(detect_ == thread);
    Attach(factory_->CreateProxySocket(detect_->proxy(), family_, type_));
    detect_->Release();
    detect_ = NULL;
    if (0 == AsyncSocketAdapter::Connect(remote_)) {
      SignalConnectEvent(this);
    } else if (!IsBlockingError(socket_->GetError())) {
      SignalCloseEvent(this, socket_->GetError());
    }
  }

  SslSocketFactory* factory_;
  int family_;
  int type_;
  SocketAddress remote_;
  AutoDetectProxy* detect_;
};

///////////////////////////////////////////////////////////////////////////////
// SslSocketFactory
///////////////////////////////////////////////////////////////////////////////

Socket* SslSocketFactory::CreateSocket(int type) {
  return CreateSocket(AF_INET, type);
}

Socket* SslSocketFactory::CreateSocket(int family, int type) {
  return factory_->CreateSocket(family, type);
}

AsyncSocket* SslSocketFactory::CreateAsyncSocket(int type) {
  return CreateAsyncSocket(AF_INET, type);
}

AsyncSocket* SslSocketFactory::CreateAsyncSocket(int family, int type) {
  if (autodetect_proxy_) {
    return new ProxySocketAdapter(this, family, type);
  } else {
    return CreateProxySocket(proxy_, family, type);
  }
}


AsyncSocket* SslSocketFactory::CreateProxySocket(const ProxyInfo& proxy,
                                                 int family,
                                                 int type) {
  AsyncSocket* socket = factory_->CreateAsyncSocket(family, type);
  if (!socket)
    return NULL;

  // Binary logging happens at the lowest level
  if (!logging_label_.empty() && binary_mode_) {
    socket = new LoggingSocketAdapter(socket, logging_level_,
                                      logging_label_.c_str(), binary_mode_);
  }

  if (proxy.type) {
    AsyncSocket* proxy_socket = 0;
    if (proxy_.type == PROXY_SOCKS5) {
      proxy_socket = new AsyncSocksProxySocket(socket, proxy.address,
                                               proxy.username, proxy.password);
    } else {
      // Note: we are trying unknown proxies as HTTPS currently
      AsyncHttpsProxySocket* http_proxy =
          new AsyncHttpsProxySocket(socket, agent_, proxy.address,
                                    proxy.username, proxy.password);
      http_proxy->SetForceConnect(force_connect_ || !hostname_.empty());
      proxy_socket = http_proxy;
    }
    if (!proxy_socket) {
      delete socket;
      return NULL;
    }
    socket = proxy_socket;  // for our purposes the proxy is now the socket
  }

  if (!hostname_.empty()) {
    if (SSLAdapter* ssl_adapter = SSLAdapter::Create(socket)) {
      ssl_adapter->set_ignore_bad_cert(ignore_bad_cert_);
      ssl_adapter->StartSSL(hostname_.c_str(), true);
      socket = ssl_adapter;
    } else {
      LOG_F(LS_ERROR) << "SSL unavailable";
    }
  }

  // Regular logging occurs at the highest level
  if (!logging_label_.empty() && !binary_mode_) {
    socket = new LoggingSocketAdapter(socket, logging_level_,
                                      logging_label_.c_str(), binary_mode_);
  }
  return socket;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base
