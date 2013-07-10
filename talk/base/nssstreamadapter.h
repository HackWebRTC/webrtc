/*
 * libjingle
 * Copyright 2004--2008, Google Inc. 
 * Copyright 2011, RTFM, Inc. 
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

#ifndef TALK_BASE_NSSSTREAMADAPTER_H_
#define TALK_BASE_NSSSTREAMADAPTER_H_

#include <string>
#include <vector>

#include "nspr.h"
#include "nss.h"
#include "secmodt.h"

#include "talk/base/buffer.h"
#include "talk/base/nssidentity.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/sslstreamadapterhelper.h"

namespace talk_base {

// Singleton
class NSSContext {
 public:
  NSSContext() {}
  ~NSSContext() {
  }

  static PK11SlotInfo *GetSlot() {
    return Instance() ? Instance()->slot_: NULL;
  }

  static NSSContext *Instance();
  static bool InitializeSSL(VerificationCallback callback);
  static bool InitializeSSLThread();
  static bool CleanupSSL();

 private:
  PK11SlotInfo *slot_;                    // The PKCS-11 slot
  static bool initialized;                // Was this initialized?
  static NSSContext *global_nss_context;  // The global context
};


class NSSStreamAdapter : public SSLStreamAdapterHelper {
 public:
  explicit NSSStreamAdapter(StreamInterface* stream);
  virtual ~NSSStreamAdapter();
  bool Init();

  virtual StreamResult Read(void* data, size_t data_len,
                            size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error);
  void OnMessage(Message *msg);

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
  // Override SSLStreamAdapter
  virtual void OnEvent(StreamInterface* stream, int events, int err);

  // Override SSLStreamAdapterHelper
  virtual int BeginSSL();
  virtual void Cleanup();
  virtual bool GetDigestLength(const std::string &algorithm,
                               std::size_t *length) {
    return NSSCertificate::GetDigestLength(algorithm, length);
  }

 private:
  int ContinueSSL();
  static SECStatus AuthCertificateHook(void *arg, PRFileDesc *fd,
                                       PRBool checksig, PRBool isServer);
  static SECStatus GetClientAuthDataHook(void *arg, PRFileDesc *fd,
                                         CERTDistNames *caNames,
                                         CERTCertificate **pRetCert,
                                         SECKEYPrivateKey **pRetKey);

  PRFileDesc *ssl_fd_;              // NSS's SSL file descriptor
  static bool initialized;          // Was InitializeSSL() called?
  bool cert_ok_;                    // Did we get and check a cert
  std::vector<PRUint16> srtp_ciphers_;  // SRTP cipher list

  static PRDescIdentity nspr_layer_identity;  // The NSPR layer identity
};

}  // namespace talk_base

#endif  // TALK_BASE_NSSSTREAMADAPTER_H_
