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
      identity1_(NULL), identity2_(NULL) {
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
