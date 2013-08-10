/*
 * libjingle
 * Copyright 2013, Google Inc.
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
 *
 */

#ifndef TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_
#define TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_

#include "talk/app/webrtc/peerconnectioninterface.h"

static const char kRSA_PRIVATE_KEY_PEM[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIICXQIBAAKBgQDCueE4a9hDMZ3sbVZdlXOz9ZA+cvzie3zJ9gXnT/BCt9P4b9HE\n"
    "vD/tr73YBqD3Wr5ZWScmyGYF9EMn0r3rzBxv6oooLU5TdUvOm4rzUjkCLQaQML8o\n"
    "NxXq+qW/j3zUKGikLhaaAl/amaX2zSWUsRQ1CpngQ3+tmDNH4/25TncNmQIDAQAB\n"
    "AoGAUcuU0Id0k10fMjYHZk4mCPzot2LD2Tr4Aznl5vFMQipHzv7hhZtx2xzMSRcX\n"
    "vG+Qr6VkbcUWHgApyWubvZXCh3+N7Vo2aYdMAQ8XqmFpBdIrL5CVdVfqFfEMlgEy\n"
    "LSZNG5klnrIfl3c7zQVovLr4eMqyl2oGfAqPQz75+fecv1UCQQD6wNHch9NbAG1q\n"
    "yuFEhMARB6gDXb+5SdzFjjtTWW5uJfm4DcZLoYyaIZm0uxOwsUKd0Rsma+oGitS1\n"
    "CXmuqfpPAkEAxszyN3vIdpD44SREEtyKZBMNOk5pEIIGdbeMJC5/XHvpxww9xkoC\n"
    "+39NbvUZYd54uT+rafbx4QZKc0h9xA/HlwJBAL37lYVWy4XpPv1olWCKi9LbUCqs\n"
    "vvQtyD1N1BkEayy9TQRsO09WKOcmigRqsTJwOx7DLaTgokEuspYvhagWVPUCQE/y\n"
    "0+YkTbYBD1Xbs9SyBKXCU6uDJRWSdO6aZi2W1XloC9gUwDMiSJjD1Wwt/YsyYPJ+\n"
    "/Hyc5yFL2l0KZimW/vkCQQCjuZ/lPcH46EuzhdbRfumDOG5N3ld7UhGI1TIRy17W\n"
    "dGF90cG33/L6BfS8Ll+fkkW/2AMRk8FDvF4CZi2nfW4L\n"
    "-----END RSA PRIVATE KEY-----\n";

static const char kCERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBmTCCAQICCQCPNJORW/M13DANBgkqhkiG9w0BAQUFADARMQ8wDQYDVQQDDAZ3\n"
    "ZWJydGMwHhcNMTMwNjE0MjIzMDAxWhcNMTQwNjE0MjIzMDAxWjARMQ8wDQYDVQQD\n"
    "DAZ3ZWJydGMwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMK54Thr2EMxnext\n"
    "Vl2Vc7P1kD5y/OJ7fMn2BedP8EK30/hv0cS8P+2vvdgGoPdavllZJybIZgX0QyfS\n"
    "vevMHG/qiigtTlN1S86bivNSOQItBpAwvyg3Fer6pb+PfNQoaKQuFpoCX9qZpfbN\n"
    "JZSxFDUKmeBDf62YM0fj/blOdw2ZAgMBAAEwDQYJKoZIhvcNAQEFBQADgYEAECMt\n"
    "UZb35H8TnjGx4XPzco/kbnurMLFFWcuve/DwTsuf10Ia9N4md8LY0UtgIgtyNqWc\n"
    "ZwyRMwxONF6ty3wcaIiPbGqiAa55T3YRuPibkRmck9CjrmM9JAtyvqHnpHd2TsBD\n"
    "qCV42aXS3onOXDQ1ibuWq0fr0//aj0wo4KV474c=\n"
    "-----END CERTIFICATE-----\n";

using webrtc::DTLSIdentityRequestObserver;

class FakeIdentityService : public webrtc::DTLSIdentityServiceInterface,
                            public talk_base::MessageHandler {
 public:
  struct Request {
    Request(const std::string& common_name,
            DTLSIdentityRequestObserver* observer)
        : common_name(common_name), observer(observer) {}

    std::string common_name;
    talk_base::scoped_refptr<DTLSIdentityRequestObserver> observer;
  };
  typedef talk_base::TypedMessageData<Request> MessageData;

  FakeIdentityService() : should_fail_(false) {}

  void set_should_fail(bool should_fail) {
    should_fail_ = should_fail;
  }

  // DTLSIdentityServiceInterface implemenation.
  virtual bool RequestIdentity(const std::string& identity_name,
                               const std::string& common_name,
                               DTLSIdentityRequestObserver* observer) {
    MessageData* msg = new MessageData(Request(common_name, observer));
    if (should_fail_) {
      talk_base::Thread::Current()->Post(this, MSG_FAILURE, msg);
    } else {
      talk_base::Thread::Current()->Post(this, MSG_SUCCESS, msg);
    }
    return true;
  }

 private:
  enum {
    MSG_SUCCESS,
    MSG_FAILURE,
  };

  // talk_base::MessageHandler implementation.
  void OnMessage(talk_base::Message* msg) {
    FakeIdentityService::MessageData* message_data =
        static_cast<FakeIdentityService::MessageData*>(msg->pdata);
    DTLSIdentityRequestObserver* observer = message_data->data().observer.get();
    switch (msg->message_id) {
      case MSG_SUCCESS: {
        std::string cert, key;
        GenerateIdentity(message_data->data().common_name, &cert, &key);
        observer->OnSuccess(cert, key);
        break;
      }
      case MSG_FAILURE:
        observer->OnFailure(0);
        break;
    }
  }

  void GenerateIdentity(
      const std::string& common_name,
      std::string* der_cert,
      std::string* der_key) {
    talk_base::SSLIdentity::PemToDer("CERTIFICATE", kCERT_PEM, der_cert);
    talk_base::SSLIdentity::PemToDer("RSA PRIVATE KEY",
                                     kRSA_PRIVATE_KEY_PEM,
                                     der_key);
  }

  bool should_fail_;
};

#endif  // TALK_APP_WEBRTC_TEST_FAKEDTLSIDENTITYSERVICE_H_
