/*
 * libjingle
 * Copyright 2011, Google Inc.
 * Portions Copyright 2011, RTFM, Inc.
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

#include <string>

#include "talk/base/gunit.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslidentity.h"

using talk_base::SSLIdentity;

const char kTestCertificate[] = "-----BEGIN CERTIFICATE-----\n"
    "MIIB6TCCAVICAQYwDQYJKoZIhvcNAQEEBQAwWzELMAkGA1UEBhMCQVUxEzARBgNV\n"
    "BAgTClF1ZWVuc2xhbmQxGjAYBgNVBAoTEUNyeXB0U29mdCBQdHkgTHRkMRswGQYD\n"
    "VQQDExJUZXN0IENBICgxMDI0IGJpdCkwHhcNMDAxMDE2MjIzMTAzWhcNMDMwMTE0\n"
    "MjIzMTAzWjBjMQswCQYDVQQGEwJBVTETMBEGA1UECBMKUXVlZW5zbGFuZDEaMBgG\n"
    "A1UEChMRQ3J5cHRTb2Z0IFB0eSBMdGQxIzAhBgNVBAMTGlNlcnZlciB0ZXN0IGNl\n"
    "cnQgKDUxMiBiaXQpMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAJ+zw4Qnlf8SMVIP\n"
    "Fe9GEcStgOY2Ww/dgNdhjeD8ckUJNP5VZkVDTGiXav6ooKXfX3j/7tdkuD8Ey2//\n"
    "Kv7+ue0CAwEAATANBgkqhkiG9w0BAQQFAAOBgQCT0grFQeZaqYb5EYfk20XixZV4\n"
    "GmyAbXMftG1Eo7qGiMhYzRwGNWxEYojf5PZkYZXvSqZ/ZXHXa4g59jK/rJNnaVGM\n"
    "k+xIX8mxQvlV0n5O9PIha5BX5teZnkHKgL8aKKLKW1BK7YTngsfSzzaeame5iKfz\n"
    "itAE+OjGF+PFKbwX8Q==\n"
    "-----END CERTIFICATE-----\n";

const unsigned char kTestCertSha1[] = {0xA6, 0xC8, 0x59, 0xEA,
                                       0xC3, 0x7E, 0x6D, 0x33,
                                       0xCF, 0xE2, 0x69, 0x9D,
                                       0x74, 0xE6, 0xF6, 0x8A,
                                       0x9E, 0x47, 0xA7, 0xCA};

class SSLIdentityTest : public testing::Test {
 public:
  SSLIdentityTest() :
      identity1_(), identity2_() {
  }

  ~SSLIdentityTest() {
  }

  static void SetUpTestCase() {
    talk_base::InitializeSSL();
  }

  static void TearDownTestCase() {
    talk_base::CleanupSSL();
  }

  virtual void SetUp() {
    identity1_.reset(SSLIdentity::Generate("test1"));
    identity2_.reset(SSLIdentity::Generate("test2"));

    ASSERT_TRUE(identity1_);
    ASSERT_TRUE(identity2_);

    test_cert_.reset(
        talk_base::SSLCertificate::FromPEMString(kTestCertificate));
    ASSERT_TRUE(test_cert_);
  }

  void TestGetSignatureDigestAlgorithm() {
    std::string digest_algorithm;
    // Both NSSIdentity::Generate and OpenSSLIdentity::Generate are
    // hard-coded to generate RSA-SHA1 certificates.
    ASSERT_TRUE(identity1_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(talk_base::DIGEST_SHA_1, digest_algorithm);
    ASSERT_TRUE(identity2_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(talk_base::DIGEST_SHA_1, digest_algorithm);

    // The test certificate has an MD5-based signature.
    ASSERT_TRUE(test_cert_->GetSignatureDigestAlgorithm(&digest_algorithm));
    ASSERT_EQ(talk_base::DIGEST_MD5, digest_algorithm);
  }

  void TestDigest(const std::string &algorithm, size_t expected_len,
                  const unsigned char *expected_digest = NULL) {
    unsigned char digest1[64];
    unsigned char digest1b[64];
    unsigned char digest2[64];
    size_t digest1_len;
    size_t digest1b_len;
    size_t digest2_len;
    bool rv;

    rv = identity1_->certificate().ComputeDigest(algorithm,
                                                 digest1, sizeof(digest1),
                                                 &digest1_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest1_len);

    rv = identity1_->certificate().ComputeDigest(algorithm,
                                                 digest1b, sizeof(digest1b),
                                                 &digest1b_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest1b_len);
    EXPECT_EQ(0, memcmp(digest1, digest1b, expected_len));


    rv = identity2_->certificate().ComputeDigest(algorithm,
                                                 digest2, sizeof(digest2),
                                                 &digest2_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest2_len);
    EXPECT_NE(0, memcmp(digest1, digest2, expected_len));

    // If we have an expected hash for the test cert, check it.
    if (expected_digest) {
      unsigned char digest3[64];
      size_t digest3_len;

      rv = test_cert_->ComputeDigest(algorithm, digest3, sizeof(digest3),
                                    &digest3_len);
      EXPECT_TRUE(rv);
      EXPECT_EQ(expected_len, digest3_len);
      EXPECT_EQ(0, memcmp(digest3, expected_digest, expected_len));
    }
  }

 private:
  talk_base::scoped_ptr<SSLIdentity> identity1_;
  talk_base::scoped_ptr<SSLIdentity> identity2_;
  talk_base::scoped_ptr<talk_base::SSLCertificate> test_cert_;
};

TEST_F(SSLIdentityTest, DigestSHA1) {
  TestDigest(talk_base::DIGEST_SHA_1, 20, kTestCertSha1);
}

// HASH_AlgSHA224 is not supported in the chromium linux build.
#if SSL_USE_NSS
TEST_F(SSLIdentityTest, DISABLED_DigestSHA224) {
#else
TEST_F(SSLIdentityTest, DigestSHA224) {
#endif
  TestDigest(talk_base::DIGEST_SHA_224, 28);
}

TEST_F(SSLIdentityTest, DigestSHA256) {
  TestDigest(talk_base::DIGEST_SHA_256, 32);
}

TEST_F(SSLIdentityTest, DigestSHA384) {
  TestDigest(talk_base::DIGEST_SHA_384, 48);
}

TEST_F(SSLIdentityTest, DigestSHA512) {
  TestDigest(talk_base::DIGEST_SHA_512, 64);
}

TEST_F(SSLIdentityTest, FromPEMStrings) {
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

  talk_base::scoped_ptr<SSLIdentity> identity(
      SSLIdentity::FromPEMStrings(kRSA_PRIVATE_KEY_PEM, kCERT_PEM));
  EXPECT_TRUE(identity);
  EXPECT_EQ(kCERT_PEM, identity->certificate().ToPEMString());
}

TEST_F(SSLIdentityTest, PemDerConversion) {
  std::string der;
  EXPECT_TRUE(SSLIdentity::PemToDer("CERTIFICATE", kTestCertificate, &der));

  EXPECT_EQ(kTestCertificate, SSLIdentity::DerToPem(
      "CERTIFICATE",
      reinterpret_cast<const unsigned char*>(der.data()), der.length()));
}

TEST_F(SSLIdentityTest, GetSignatureDigestAlgorithm) {
  TestGetSignatureDigestAlgorithm();
}
