/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#ifndef TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_
#define TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_

#include <string>

#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/rtccertificate.h"

static const char kRSA_PRIVATE_KEY_PEM[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAMYRkbhmI7kVA/rM\n"
    "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
    "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
    "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAECgYAvgOs4FJcgvp+TuREx7YtiYVsH\n"
    "mwQPTum2z/8VzWGwR8BBHBvIpVe1MbD/Y4seyI2aco/7UaisatSgJhsU46/9Y4fq\n"
    "2TwXH9QANf4at4d9n/R6rzwpAJOpgwZgKvdQjkfrKTtgLV+/dawvpxUYkRH4JZM1\n"
    "CVGukMfKNrSVH4Ap4QJBAOJmGV1ASPnB4r4nc99at7JuIJmd7fmuVUwUgYi4XgaR\n"
    "WhScBsgYwZ/JoywdyZJgnbcrTDuVcWG56B3vXbhdpMsCQQDf9zeJrjnPZ3Cqm79y\n"
    "kdqANep0uwZciiNiWxsQrCHztywOvbFhdp8iYVFG9EK8DMY41Y5TxUwsHD+67zao\n"
    "ZNqJAkEA1suLUP/GvL8IwuRneQd2tWDqqRQ/Td3qq03hP7e77XtF/buya3Ghclo5\n"
    "54czUR89QyVfJEC6278nzA7n2h1uVQJAcG6mztNL6ja/dKZjYZye2CY44QjSlLo0\n"
    "MTgTSjdfg/28fFn2Jjtqf9Pi/X+50LWI/RcYMC2no606wRk9kyOuIQJBAK6VSAim\n"
    "1pOEjsYQn0X5KEIrz1G3bfCbB848Ime3U2/FWlCHMr6ch8kCZ5d1WUeJD3LbwMNG\n"
    "UCXiYxSsu20QNVw=\n"
    "-----END RSA PRIVATE KEY-----\n";

static const char kCERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBmTCCAQKgAwIBAgIEbzBSAjANBgkqhkiG9w0BAQsFADARMQ8wDQYDVQQDEwZX\n"
    "ZWJSVEMwHhcNMTQwMTAyMTgyNDQ3WhcNMTQwMjAxMTgyNDQ3WjARMQ8wDQYDVQQD\n"
    "EwZXZWJSVEMwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMYRkbhmI7kVA/rM\n"
    "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
    "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
    "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAEwDQYJKoZIhvcNAQELBQADgYEAUflI\n"
    "VUe5Krqf5RVa5C3u/UTAOAUJBiDS3VANTCLBxjuMsvqOG0WvaYWP3HYPgrz0jXK2\n"
    "LJE/mGw3MyFHEqi81jh95J+ypl6xKW6Rm8jKLR87gUvCaVYn/Z4/P3AqcQTB7wOv\n"
    "UD0A8qfhfDM+LK6rPAnCsVN0NRDY3jvd6rzix9M=\n"
    "-----END CERTIFICATE-----\n";

class FakeDtlsIdentityStore : public webrtc::DtlsIdentityStoreInterface,
                              public rtc::MessageHandler {
 public:
  typedef rtc::TypedMessageData<rtc::scoped_refptr<
      webrtc::DtlsIdentityRequestObserver> > MessageData;

  FakeDtlsIdentityStore() : should_fail_(false) {}

  void set_should_fail(bool should_fail) {
    should_fail_ = should_fail;
  }

  void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>&
          observer) override {
    // TODO(hbos): Should be able to generate KT_ECDSA too.
    DCHECK(key_type == rtc::KT_RSA || should_fail_);
    MessageData* msg = new MessageData(
        rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>(observer));
    rtc::Thread::Current()->Post(
        this, should_fail_ ? MSG_FAILURE : MSG_SUCCESS, msg);
  }

  static rtc::scoped_refptr<rtc::RTCCertificate> GenerateCertificate() {
    std::string cert;
    std::string key;
    rtc::SSLIdentity::PemToDer("CERTIFICATE", kCERT_PEM, &cert);
    rtc::SSLIdentity::PemToDer("RSA PRIVATE KEY", kRSA_PRIVATE_KEY_PEM, &key);

    std::string pem_cert = rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeCertificate,
        reinterpret_cast<const unsigned char*>(cert.data()),
        cert.length());
    std::string pem_key = rtc::SSLIdentity::DerToPem(
        rtc::kPemTypeRsaPrivateKey,
        reinterpret_cast<const unsigned char*>(key.data()),
        key.length());
    rtc::scoped_ptr<rtc::SSLIdentity> identity(
        rtc::SSLIdentity::FromPEMStrings(pem_key, pem_cert));

    return rtc::RTCCertificate::Create(identity.Pass());
  }

 private:
  enum {
    MSG_SUCCESS,
    MSG_FAILURE,
  };

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) {
    MessageData* message_data = static_cast<MessageData*>(msg->pdata);
    rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver> observer =
        message_data->data();
    switch (msg->message_id) {
      case MSG_SUCCESS: {
        std::string cert;
        std::string key;
        rtc::SSLIdentity::PemToDer("CERTIFICATE", kCERT_PEM, &cert);
        rtc::SSLIdentity::PemToDer("RSA PRIVATE KEY", kRSA_PRIVATE_KEY_PEM,
                                   &key);
        observer->OnSuccess(cert, key);
        break;
      }
      case MSG_FAILURE:
        observer->OnFailure(0);
        break;
    }
    delete message_data;
  }

  bool should_fail_;
};

#endif  // TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_
