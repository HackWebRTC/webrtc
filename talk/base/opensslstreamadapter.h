/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#ifndef TALK_BASE_OPENSSLSTREAMADAPTER_H__
#define TALK_BASE_OPENSSLSTREAMADAPTER_H__

#include <string>
#include <vector>

#include "talk/base/buffer.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/opensslidentity.h"

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_store_ctx_st X509_STORE_CTX;

namespace talk_base {

// This class was written with OpenSSLAdapter (a socket adapter) as a
// starting point. It has similar structure and functionality, with
// the peer-to-peer mode added.
//
// Static methods to initialize and deinit the SSL library are in
// OpenSSLAdapter. This class also uses
// OpenSSLAdapter::custom_verify_callback_ (a static field). These
// should probably be moved out to a neutral class.
//
// In a few cases I have factored out some OpenSSLAdapter code into
// static methods so it can be reused from this class. Eventually that
// code should probably be moved to a common support
// class. Unfortunately there remain a few duplicated sections of
// code. I have not done more restructuring because I did not want to
// affect existing code that uses OpenSSLAdapter.
//
// This class does not support the SSL connection restart feature
// present in OpenSSLAdapter. I am not entirely sure how the feature
// is useful and I am not convinced that it works properly.
//
// This implementation is careful to disallow data exchange after an
// SSL error, and it has an explicit SSL_CLOSED state. It should not
// be possible to send any data in clear after one of the StartSSL
// methods has been called.

// Look in sslstreamadapter.h for documentation of the methods.

class OpenSSLIdentity;

///////////////////////////////////////////////////////////////////////////////

class OpenSSLStreamAdapter : public SSLStreamAdapter {
 public:
  explicit OpenSSLStreamAdapter(StreamInterface* stream);
  virtual ~OpenSSLStreamAdapter();

  virtual void SetIdentity(SSLIdentity* identity);

  // Default argument is for compatibility
  virtual void SetServerRole(SSLRole role = SSL_SERVER);
  virtual void SetPeerCertificate(SSLCertificate* cert);
  virtual bool SetPeerCertificateDigest(const std::string& digest_alg,
                                        const unsigned char* digest_val,
                                        size_t digest_len);

  virtual bool GetPeerCertificate(SSLCertificate** cert) const;

  virtual int StartSSLWithServer(const char* server_name);
  virtual int StartSSLWithPeer();
  virtual void SetMode(SSLMode mode);

  virtual StreamResult Read(void* data, size_t data_len,
                            size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error);
  virtual void Close();
  virtual StreamState GetState() const;

  // Key Extractor interface
  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8* result,
                                    size_t result_len);


  // DTLS-SRTP interface
  virtual bool SetDtlsSrtpCiphers(const std::vector<std::string>& ciphers);
  virtual bool GetDtlsSrtpCipher(std::string* cipher);

  // Capabilities interfaces
  static bool HaveDtls();
  static bool HaveDtlsSrtp();
  static bool HaveExporter();

 protected:
  virtual void OnEvent(StreamInterface* stream, int events, int err);

 private:
  enum SSLState {
    // Before calling one of the StartSSL methods, data flows
    // in clear text.
    SSL_NONE,
    SSL_WAIT,  // waiting for the stream to open to start SSL negotiation
    SSL_CONNECTING,  // SSL negotiation in progress
    SSL_CONNECTED,  // SSL stream successfully established
    SSL_ERROR,  // some SSL error occurred, stream is closed
    SSL_CLOSED  // Clean close
  };

  enum { MSG_TIMEOUT = MSG_MAX+1};

  // The following three methods return 0 on success and a negative
  // error code on failure. The error code may be from OpenSSL or -1
  // on some other error cases, so it can't really be interpreted
  // unfortunately.

  // Go from state SSL_NONE to either SSL_CONNECTING or SSL_WAIT,
  // depending on whether the underlying stream is already open or
  // not.
  int StartSSL();
  // Prepare SSL library, state is SSL_CONNECTING.
  int BeginSSL();
  // Perform SSL negotiation steps.
  int ContinueSSL();

  // Error handler helper. signal is given as true for errors in
  // asynchronous contexts (when an error method was not returned
  // through some other method), and in that case an SE_CLOSE event is
  // raised on the stream with the specified error.
  // A 0 error means a graceful close, otherwise there is not really enough
  // context to interpret the error code.
  void Error(const char* context, int err, bool signal);
  void Cleanup();

  // Override MessageHandler
  virtual void OnMessage(Message* msg);

  // Flush the input buffers by reading left bytes (for DTLS)
  void FlushInput(unsigned int left);

  // SSL library configuration
  SSL_CTX* SetupSSLContext();
  // SSL verification check
  bool SSLPostConnectionCheck(SSL* ssl, const char* server_name,
                              const X509* peer_cert,
                              const std::string& peer_digest);
  // SSL certification verification error handler, called back from
  // the openssl library. Returns an int interpreted as a boolean in
  // the C style: zero means verification failure, non-zero means
  // passed.
  static int SSLVerifyCallback(int ok, X509_STORE_CTX* store);


  SSLState state_;
  SSLRole role_;
  int ssl_error_code_;  // valid when state_ == SSL_ERROR or SSL_CLOSED
  // Whether the SSL negotiation is blocked on needing to read or
  // write to the wrapped stream.
  bool ssl_read_needs_write_;
  bool ssl_write_needs_read_;

  SSL* ssl_;
  SSL_CTX* ssl_ctx_;

  // Our key and certificate, mostly useful in peer-to-peer mode.
  scoped_ptr<OpenSSLIdentity> identity_;
  // in traditional mode, the server name that the server's certificate
  // must specify. Empty in peer-to-peer mode.
  std::string ssl_server_name_;
  // The certificate that the peer must present or did present. Initially
  // null in traditional mode, until the connection is established.
  scoped_ptr<OpenSSLCertificate> peer_certificate_;
  // In peer-to-peer mode, the digest of the certificate that
  // the peer must present.
  Buffer peer_certificate_digest_value_;
  std::string peer_certificate_digest_algorithm_;

  // OpenSSLAdapter::custom_verify_callback_ result
  bool custom_verification_succeeded_;

  // The DtlsSrtp ciphers
  std::string srtp_ciphers_;

  // Do DTLS or not
  SSLMode ssl_mode_;
};

/////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // TALK_BASE_OPENSSLSTREAMADAPTER_H__
