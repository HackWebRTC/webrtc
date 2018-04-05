/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSLADAPTER_H_
#define RTC_BASE_OPENSSLADAPTER_H_

#include <openssl/ossl_typ.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/buffer.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/opensslidentity.h"
#include "rtc_base/opensslsessioncache.h"
#include "rtc_base/ssladapter.h"

namespace rtc {

class OpenSSLAdapter : public SSLAdapter, public MessageHandler {
 public:
  static bool InitializeSSL(VerificationCallback callback);
  static bool CleanupSSL();

  explicit OpenSSLAdapter(AsyncSocket* socket,
                          OpenSSLSessionCache* ssl_session_cache = nullptr);
  ~OpenSSLAdapter() override;

  void SetIgnoreBadCert(bool ignore) override;
  void SetAlpnProtocols(const std::vector<std::string>& protos) override;
  void SetEllipticCurves(const std::vector<std::string>& curves) override;

  void SetMode(SSLMode mode) override;
  void SetIdentity(SSLIdentity* identity) override;
  void SetRole(SSLRole role) override;
  AsyncSocket* Accept(SocketAddress* paddr) override;
  int StartSSL(const char* hostname, bool restartable) override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv, size_t cb, const SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               SocketAddress* paddr,
               int64_t* timestamp) override;
  int Close() override;

  // Note that the socket returns ST_CONNECTING while SSL is being negotiated.
  ConnState GetState() const override;
  bool IsResumedSession() override;

  // Creates a new SSL_CTX object, configured for client-to-server usage
  // with SSLMode |mode|, and if |enable_cache| is true, with support for
  // storing successful sessions so that they can be later resumed.
  // OpenSSLAdapterFactory will call this method to create its own internal
  // SSL_CTX, and OpenSSLAdapter will also call this when used without a
  // factory.
  static SSL_CTX* CreateContext(SSLMode mode, bool enable_cache);

 protected:
  void OnConnectEvent(AsyncSocket* socket) override;
  void OnReadEvent(AsyncSocket* socket) override;
  void OnWriteEvent(AsyncSocket* socket) override;
  void OnCloseEvent(AsyncSocket* socket, int err) override;

 private:
  enum SSLState {
    SSL_NONE, SSL_WAIT, SSL_CONNECTING, SSL_CONNECTED, SSL_ERROR
  };

  enum { MSG_TIMEOUT };

  int BeginSSL();
  int ContinueSSL();
  void Error(const char* context, int err, bool signal = true);
  void Cleanup();

  // Return value and arguments have the same meanings as for Send; |error| is
  // an output parameter filled with the result of SSL_get_error.
  int DoSslWrite(const void* pv, size_t cb, int* error);

  void OnMessage(Message* msg) override;

  bool SSLPostConnectionCheck(SSL* ssl, const std::string& host);

#if !defined(NDEBUG)
  // In debug builds, logs info about the state of the SSL connection.
  static void SSLInfoCallback(const SSL* ssl, int where, int ret);
#endif
  static int SSLVerifyCallback(int ok, X509_STORE_CTX* store);
  static VerificationCallback custom_verify_callback_;
  friend class OpenSSLStreamAdapter;  // for custom_verify_callback_;

  // If the SSL_CTX was created with |enable_cache| set to true, this callback
  // will be called when a SSL session has been successfully established,
  // to allow its SSL_SESSION* to be cached for later resumption.
  static int NewSSLSessionCallback(SSL* ssl, SSL_SESSION* session);

  static bool ConfigureTrustedRootCertificates(SSL_CTX* ctx);

  // Parent object that maintains shared state.
  // Can be null if state sharing is not needed.
  OpenSSLSessionCache* ssl_session_cache_ = nullptr;

  SSLState state_;
  std::unique_ptr<OpenSSLIdentity> identity_;
  SSLRole role_;
  bool ssl_read_needs_write_;
  bool ssl_write_needs_read_;
  // If true, socket will retain SSL configuration after Close.
  // TODO(juberti): Remove this unused flag.
  bool restartable_;

  // This buffer is used if SSL_write fails with SSL_ERROR_WANT_WRITE, which
  // means we need to keep retrying with *the same exact data* until it
  // succeeds. Afterwards it will be cleared.
  Buffer pending_data_;

  SSL* ssl_;
  SSL_CTX* ssl_ctx_;
  std::string ssl_host_name_;
  // Do DTLS or not
  SSLMode ssl_mode_;
  // If true, the server certificate need not match the configured hostname.
  bool ignore_bad_cert_;
  // List of protocols to be used in the TLS ALPN extension.
  std::vector<std::string> alpn_protocols_;
  // List of elliptic curves to be used in the TLS elliptic curves extension.
  std::vector<std::string> elliptic_curves_;

  bool custom_verification_succeeded_;
};

std::string TransformAlpnProtocols(const std::vector<std::string>& protos);

/////////////////////////////////////////////////////////////////////////////

// The OpenSSLAdapterFactory is responsbile for creating multiple new
// OpenSSLAdapters with a shared SSL_CTX and a shared SSL_SESSION cache. The
// SSL_SESSION cache allows existing SSL_SESSIONS to be reused instead of
// recreating them leading to a significant performance improvement.
class OpenSSLAdapterFactory : public SSLAdapterFactory {
 public:
  OpenSSLAdapterFactory();
  ~OpenSSLAdapterFactory() override;
  // Set the SSL Mode to use with this factory. This should only be set before
  // the first adapter is created with the factory. If it is called after it
  // will DCHECK.
  void SetMode(SSLMode mode) override;
  // Constructs a new socket using the shared OpenSSLSessionCache. This means
  // existing SSLSessions already in the cache will be reused instead of
  // re-created for improved performance.
  OpenSSLAdapter* CreateAdapter(AsyncSocket* socket) override;

 private:
  // Holds the SSLMode (DTLS,TLS) that will be used to set the session cache.
  SSLMode ssl_mode_ = SSL_MODE_TLS;
  // Holds a cache of existing SSL Sessions.
  std::unique_ptr<OpenSSLSessionCache> ssl_session_cache_;
  // TODO(benwright): Remove this when context is moved to OpenSSLCommon.
  // Hold a friend class to the OpenSSLAdapter to retrieve the context.
  friend class OpenSSLAdapter;
};

}  // namespace rtc

#endif  // RTC_BASE_OPENSSLADAPTER_H_
