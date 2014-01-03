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

#ifndef TALK_BASE_SSLSTREAMADAPTER_H_
#define TALK_BASE_SSLSTREAMADAPTER_H_

#include <string>
#include <vector>

#include "talk/base/stream.h"
#include "talk/base/sslidentity.h"

namespace talk_base {

// SSLStreamAdapter : A StreamInterfaceAdapter that does SSL/TLS.
// After SSL has been started, the stream will only open on successful
// SSL verification of certificates, and the communication is
// encrypted of course.
//
// This class was written with SSLAdapter as a starting point. It
// offers a similar interface, with two differences: there is no
// support for a restartable SSL connection, and this class has a
// peer-to-peer mode.
//
// The SSL library requires initialization and cleanup. Static method
// for doing this are in SSLAdapter. They should possibly be moved out
// to a neutral class.


enum SSLRole { SSL_CLIENT, SSL_SERVER };
enum SSLMode { SSL_MODE_TLS, SSL_MODE_DTLS };

// Errors for Read -- in the high range so no conflict with OpenSSL.
enum { SSE_MSG_TRUNC = 0xff0001 };

class SSLStreamAdapter : public StreamAdapterInterface {
 public:
  // Instantiate an SSLStreamAdapter wrapping the given stream,
  // (using the selected implementation for the platform).
  // Caller is responsible for freeing the returned object.
  static SSLStreamAdapter* Create(StreamInterface* stream);

  explicit SSLStreamAdapter(StreamInterface* stream)
      : StreamAdapterInterface(stream), ignore_bad_cert_(false) { }

  void set_ignore_bad_cert(bool ignore) { ignore_bad_cert_ = ignore; }
  bool ignore_bad_cert() const { return ignore_bad_cert_; }

  // Specify our SSL identity: key and certificate. Mostly this is
  // only used in the peer-to-peer mode (unless we actually want to
  // provide a client certificate to a server).
  // SSLStream takes ownership of the SSLIdentity object and will
  // free it when appropriate. Should be called no more than once on a
  // given SSLStream instance.
  virtual void SetIdentity(SSLIdentity* identity) = 0;

  // Call this to indicate that we are to play the server's role in
  // the peer-to-peer mode.
  // The default argument is for backward compatibility
  // TODO(ekr@rtfm.com): rename this SetRole to reflect its new function
  virtual void SetServerRole(SSLRole role = SSL_SERVER) = 0;

  // Do DTLS or TLS
  virtual void SetMode(SSLMode mode) = 0;

  // The mode of operation is selected by calling either
  // StartSSLWithServer or StartSSLWithPeer.
  // Use of the stream prior to calling either of these functions will
  // pass data in clear text.
  // Calling one of these functions causes SSL negotiation to begin as
  // soon as possible: right away if the underlying wrapped stream is
  // already opened, or else as soon as it opens.
  //
  // These functions return a negative error code on failure.
  // Returning 0 means success so far, but negotiation is probably not
  // complete and will continue asynchronously.  In that case, the
  // exposed stream will open after successful negotiation and
  // verification, or an SE_CLOSE event will be raised if negotiation
  // fails.

  // StartSSLWithServer starts SSL negotiation with a server in
  // traditional mode. server_name specifies the expected server name
  // which the server's certificate needs to specify.
  virtual int StartSSLWithServer(const char* server_name) = 0;

  // StartSSLWithPeer starts negotiation in the special peer-to-peer
  // mode.
  // Generally, SetIdentity() and possibly SetServerRole() should have
  // been called before this.
  // SetPeerCertificate() or SetPeerCertificateDigest() must also be called.
  // It may be called after StartSSLWithPeer() but must be called before the
  // underlying stream opens.
  virtual int StartSSLWithPeer() = 0;

  // Specify the digest of the certificate that our peer is expected to use in
  // peer-to-peer mode. Only this certificate will be accepted during
  // SSL verification. The certificate is assumed to have been
  // obtained through some other secure channel (such as the XMPP
  // channel). Unlike SetPeerCertificate(), this must specify the
  // terminal certificate, not just a CA.
  // SSLStream makes a copy of the digest value.
  virtual bool SetPeerCertificateDigest(const std::string& digest_alg,
                                        const unsigned char* digest_val,
                                        size_t digest_len) = 0;

  // Retrieves the peer's X.509 certificate, if a connection has been
  // established. It returns the transmitted over SSL, including the entire
  // chain. The returned certificate is owned by the caller.
  virtual bool GetPeerCertificate(SSLCertificate** cert) const = 0;

  // Key Exporter interface from RFC 5705
  // Arguments are:
  // label               -- the exporter label.
  //                        part of the RFC defining each exporter
  //                        usage (IN)
  // context/context_len -- a context to bind to for this connection;
  //                        optional, can be NULL, 0 (IN)
  // use_context         -- whether to use the context value
  //                        (needed to distinguish no context from
  //                        zero-length ones).
  // result              -- where to put the computed value
  // result_len          -- the length of the computed value
  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8* result,
                                    size_t result_len) {
    return false;  // Default is unsupported
  }


  // DTLS-SRTP interface
  virtual bool SetDtlsSrtpCiphers(const std::vector<std::string>& ciphers) {
    return false;
  }

  virtual bool GetDtlsSrtpCipher(std::string* cipher) {
    return false;
  }

  // Capabilities testing
  static bool HaveDtls();
  static bool HaveDtlsSrtp();
  static bool HaveExporter();

  // If true, the server certificate need not match the configured
  // server_name, and in fact missing certificate authority and other
  // verification errors are ignored.
  bool ignore_bad_cert_;
};

}  // namespace talk_base

#endif  // TALK_BASE_SSLSTREAMADAPTER_H_
