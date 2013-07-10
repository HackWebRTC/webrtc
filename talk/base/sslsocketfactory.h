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

#ifndef TALK_BASE_SSLSOCKETFACTORY_H__
#define TALK_BASE_SSLSOCKETFACTORY_H__

#include "talk/base/proxyinfo.h"
#include "talk/base/socketserver.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// SslSocketFactory
///////////////////////////////////////////////////////////////////////////////

class SslSocketFactory : public SocketFactory {
 public:
  SslSocketFactory(SocketFactory* factory, const std::string& user_agent)
     : factory_(factory), agent_(user_agent), autodetect_proxy_(true),
       force_connect_(false), logging_level_(LS_VERBOSE), binary_mode_(false),
       ignore_bad_cert_(false) {
  }

  void SetAutoDetectProxy() {
    autodetect_proxy_ = true;
  }
  void SetForceConnect(bool force) {
    force_connect_ = force;
  }
  void SetProxy(const ProxyInfo& proxy) {
    autodetect_proxy_ = false;
    proxy_ = proxy;
  }
  bool autodetect_proxy() const { return autodetect_proxy_; }
  const ProxyInfo& proxy() const { return proxy_; }

  void UseSSL(const char* hostname) { hostname_ = hostname; }
  void DisableSSL() { hostname_.clear(); }
  void SetIgnoreBadCert(bool ignore) { ignore_bad_cert_ = ignore; }
  bool ignore_bad_cert() const { return ignore_bad_cert_; }

  void SetLogging(LoggingSeverity level, const std::string& label, 
                  bool binary_mode = false) {
    logging_level_ = level;
    logging_label_ = label;
    binary_mode_ = binary_mode;
  }

  // SocketFactory Interface
  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

 private:
  friend class ProxySocketAdapter;
  AsyncSocket* CreateProxySocket(const ProxyInfo& proxy, int family, int type);

  SocketFactory* factory_;
  std::string agent_;
  bool autodetect_proxy_, force_connect_;
  ProxyInfo proxy_;
  std::string hostname_, logging_label_;
  LoggingSeverity logging_level_;
  bool binary_mode_;
  bool ignore_bad_cert_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // TALK_BASE_SSLSOCKETFACTORY_H__
