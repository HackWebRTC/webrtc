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
#include <utility>

#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/rtccertificate.h"

static const struct {
  const char* rsa_private_key_pem;
  const char* cert_pem;
} kKeysAndCerts[] = {
    {"-----BEGIN RSA PRIVATE KEY-----\n"
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
     "-----END RSA PRIVATE KEY-----\n",
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
     "-----END CERTIFICATE-----\n"},
    {"-----BEGIN RSA PRIVATE KEY-----\n"
     "MIICXQIBAAKBgQDeYqlyJ1wuiMsi905e3X81/WA/G3ym50PIDZBVtSwZi7JVQPgj\n"
     "Bl8CPZMvDh9EwB4Ji9ytA8dZZbQ4WbJWPr73zPpJSCvQqz6sOXSlenBRi72acNaQ\n"
     "sOR/qPvviJx5I6Hqo4qemfnjZhAW85a5BpgrAwKgMLIQTHCTLWwVSyrDrwIDAQAB\n"
     "AoGARni9eY8/hv+SX+I+05EdXt6MQXNUbQ+cSykBNCfVccLzIFEWUQMT2IHqwl6X\n"
     "ShIXcq7/n1QzOAEiuzixauM3YHg4xZ1Um2Ha9a7ig5Xg4v6b43bmMkNE6LkoAtYs\n"
     "qnQdfMh442b1liDud6IMb1Qk0amt3fSrgRMc547TZQVx4QECQQDxUeDm94r3p4ng\n"
     "5rCLLC1K5/6HSTZsh7jatKPlz7GfP/IZlYV7iE5784/n0wRiCjZOS7hQRy/8m2Gp\n"
     "pf4aZq+DAkEA6+np4d36FYikydvUrupLT3FkdRHGn/v83qOll/VmeNh+L1xMZlIP\n"
     "tM26hAXCcQb7O5+J9y3cx2CAQsBS11ZXZQJAfGgTo76WG9p5UEJdXUInD2jOZPwv\n"
     "XIATolxh6kXKcijLLLlSmT7KB0inNYIpzkkpee+7U1d/u6B3FriGaSHq9QJBAM/J\n"
     "ICnDdLCgwNvWVraVQC3BpwSB2pswvCFwq7py94V60XFvbw80Ogc6qIv98qvQxVlX\n"
     "hJIEgA/PjEi+0ng94Q0CQQDm8XSDby35gmjO+6eRmJtAjtB7nguLvrPXM6CPXRmD\n"
     "sRoBocpHw6j9UdzZ6qYG0FkdXZghezXFY58ro2BYYRR3\n"
     "-----END RSA PRIVATE KEY-----\n",
     "-----BEGIN CERTIFICATE-----\n"
     "MIICWDCCAcGgAwIBAgIJALgDjxMbBOhbMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
     "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
     "aWRnaXRzIFB0eSBMdGQwHhcNMTUxMTEzMjIzMjEzWhcNMTYxMTEyMjIzMjEzWjBF\n"
     "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n"
     "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n"
     "gQDeYqlyJ1wuiMsi905e3X81/WA/G3ym50PIDZBVtSwZi7JVQPgjBl8CPZMvDh9E\n"
     "wB4Ji9ytA8dZZbQ4WbJWPr73zPpJSCvQqz6sOXSlenBRi72acNaQsOR/qPvviJx5\n"
     "I6Hqo4qemfnjZhAW85a5BpgrAwKgMLIQTHCTLWwVSyrDrwIDAQABo1AwTjAdBgNV\n"
     "HQ4EFgQUx2tbJdlcSTCepn09UdYORXKuSTAwHwYDVR0jBBgwFoAUx2tbJdlcSTCe\n"
     "pn09UdYORXKuSTAwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOBgQAmp9Id\n"
     "E716gHMqeBG4S2FCgVFCr0a0ugkaneQAN/c2L9CbMemEN9W6jvucUIVOtYd90dDW\n"
     "lXuowWmT/JctPe3D2qt4yvYW3puECHk2tVQmrJOZiZiTRtWm6HxkmoUYHYp/DtaS\n"
     "1Xe29gSTnZtI5sQCrGMzk3SGRSSs7ejLKiVDBQ==\n"
     "-----END CERTIFICATE-----\n"}};

class FakeDtlsIdentityStore : public webrtc::DtlsIdentityStoreInterface,
                              public rtc::MessageHandler {
 public:
  typedef rtc::TypedMessageData<rtc::scoped_refptr<
      webrtc::DtlsIdentityRequestObserver> > MessageData;

  FakeDtlsIdentityStore() : should_fail_(false) {}

  void set_should_fail(bool should_fail) {
    should_fail_ = should_fail;
  }

  void use_original_key() { key_index_ = 0; }
  void use_alternate_key() { key_index_ = 1; }

  void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>&
          observer) override {
    // TODO(hbos): Should be able to generate KT_ECDSA too.
    RTC_DCHECK(key_type == rtc::KT_RSA || should_fail_);
    MessageData* msg = new MessageData(
        rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>(observer));
    rtc::Thread::Current()->Post(
        this, should_fail_ ? MSG_FAILURE : MSG_SUCCESS, msg);
  }

  static rtc::scoped_refptr<rtc::RTCCertificate> GenerateCertificate() {
    std::string cert;
    std::string key;
    rtc::SSLIdentity::PemToDer("CERTIFICATE", kKeysAndCerts[0].cert_pem, &cert);
    rtc::SSLIdentity::PemToDer("RSA PRIVATE KEY",
                               kKeysAndCerts[0].rsa_private_key_pem, &key);

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

    return rtc::RTCCertificate::Create(std::move(identity));
  }

 private:
  enum {
    MSG_SUCCESS,
    MSG_FAILURE,
  };

  const char* get_key() {
    return kKeysAndCerts[key_index_].rsa_private_key_pem;
  }
  const char* get_cert() { return kKeysAndCerts[key_index_].cert_pem; }

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) {
    MessageData* message_data = static_cast<MessageData*>(msg->pdata);
    rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver> observer =
        message_data->data();
    switch (msg->message_id) {
      case MSG_SUCCESS: {
        std::string cert;
        std::string key;
        rtc::SSLIdentity::PemToDer("CERTIFICATE", get_cert(), &cert);
        rtc::SSLIdentity::PemToDer("RSA PRIVATE KEY", get_key(), &key);
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
  int key_index_ = 0;
};

#endif  // TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_
