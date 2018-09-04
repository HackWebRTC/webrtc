/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSLADAPTER_H_
#define RTC_BASE_SSLADAPTER_H_

#include <string>
#include <vector>

#include "rtc_base/asyncsocket.h"
#include "rtc_base/sslcertificate.h"
#include "rtc_base/sslstreamadapter.h"

namespace rtc {

class SSLAdapter;

// TLS certificate policy.
enum class TlsCertPolicy {
  // For TLS based protocols, ensure the connection is secure by not
  // circumventing certificate validation.
  TLS_CERT_POLICY_SECURE,
  // For TLS based protocols, disregard security completely by skipping
  // certificate validation. This is insecure and should never be used unless
  // security is irrelevant in that particular context.
  // Do not set to this value in production code.
  // TODO(juberti): Remove the opportunistic encryption mechanism in
  // BasicPacketSocketFactory that uses this value.
  TLS_CERT_POLICY_INSECURE_NO_CHECK,
};

// SSL configuration options.
struct SSLConfig final {
  SSLConfig();
  SSLConfig(const SSLConfig&);
  ~SSLConfig();

  bool operator==(const SSLConfig& o) const {
    return enable_ocsp_stapling == o.enable_ocsp_stapling &&
           enable_signed_cert_timestamp == o.enable_signed_cert_timestamp &&
           enable_tls_channel_id == o.enable_tls_channel_id &&
           enable_grease == o.enable_grease &&
           max_ssl_version == o.max_ssl_version &&
           tls_alpn_protocols == o.tls_alpn_protocols &&
           tls_elliptic_curves == o.tls_elliptic_curves;
  }
  bool operator!=(const SSLConfig& o) const { return !(*this == o); }

  // If true, enables the (unused) OCSP stapling TLS extension.
  bool enable_ocsp_stapling = true;
  // If true, enables the (unused) signed certificate timestamp TLS extension.
  bool enable_signed_cert_timestamp = true;
  // If true, enables the (unused) channel ID TLS extension.
  bool enable_tls_channel_id = false;
  // If true, enables the (unused) GREASE TLS extension.
  bool enable_grease = false;
  // Indicates how to process incoming certificates.
  TlsCertPolicy tls_cert_policy = TlsCertPolicy::TLS_CERT_POLICY_SECURE;
  // If set, indicates the highest supported SSL version.
  absl::optional<int> max_ssl_version;
  // If set, indicates the list of protocols to be used in the TLS ALPN
  // extension.
  absl::optional<std::vector<std::string>> tls_alpn_protocols;
  // If set, indicates the list of curves to be used in the TLS elliptic curves
  // extension.
  absl::optional<std::vector<std::string>> tls_elliptic_curves;
};

// Class for creating SSL adapters with shared state, e.g., a session cache,
// which allows clients to resume SSL sessions to previously-contacted hosts.
// Clients should create the factory using Create(), set up the factory as
// needed using SetMode, and then call CreateAdapter to create adapters when
// needed.
class SSLAdapterFactory {
 public:
  virtual ~SSLAdapterFactory() {}

  // Specifies whether TLS or DTLS is to be used for the SSL adapters.
  virtual void SetMode(SSLMode mode) = 0;

  // Specify a custom certificate verifier for SSL.
  virtual void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) = 0;

  // Creates a new SSL adapter, but from a shared context.
  virtual SSLAdapter* CreateAdapter(AsyncSocket* socket) = 0;

  static SSLAdapterFactory* Create();
};

// Class that abstracts a client-to-server SSL session. It can be created
// standalone, via SSLAdapter::Create, or through a factory as described above,
// in which case it will share state with other SSLAdapters created from the
// same factory.
// After creation, call StartSSL to initiate the SSL handshake to the server.
class SSLAdapter : public AsyncSocketAdapter {
 public:
  explicit SSLAdapter(AsyncSocket* socket) : AsyncSocketAdapter(socket) {}

  // Sets the SSL configuration for this session.
  virtual void SetSSLConfig(const SSLConfig& ssl_config) = 0;

  // Do DTLS or TLS (default is TLS, if unspecified)
  virtual void SetMode(SSLMode mode) = 0;
  // Specify a custom certificate verifier for SSL.
  virtual void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) = 0;

  // Set the certificate this socket will present to incoming clients.
  virtual void SetIdentity(SSLIdentity* identity) = 0;

  // Choose whether the socket acts as a server socket or client socket.
  virtual void SetRole(SSLRole role) = 0;

  // StartSSL returns 0 if successful.
  // If StartSSL is called while the socket is closed or connecting, the SSL
  // negotiation will begin as soon as the socket connects.
  // TODO(juberti): Remove |restartable|.
  virtual int StartSSL(const char* hostname, bool restartable = false) = 0;

  // When an SSLAdapterFactory is used, an SSLAdapter may be used to resume
  // a previous SSL session, which results in an abbreviated handshake.
  // This method, if called after SSL has been established for this adapter,
  // indicates whether the current session is a resumption of a previous
  // session.
  virtual bool IsResumedSession() = 0;

  // Create the default SSL adapter for this platform. On failure, returns null
  // and deletes |socket|. Otherwise, the returned SSLAdapter takes ownership
  // of |socket|.
  static SSLAdapter* Create(AsyncSocket* socket);
};

///////////////////////////////////////////////////////////////////////////////

// Call this on the main thread, before using SSL.
// Call CleanupSSL when finished with SSL.
bool InitializeSSL();

// Call to cleanup additional threads, and also the main thread.
bool CleanupSSL();

}  // namespace rtc

#endif  // RTC_BASE_SSLADAPTER_H_
