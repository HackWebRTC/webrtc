/*
 * libjingle
 * Copyright 2012, Google Inc.
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
#include <vector>

#include "talk/base/fakesslidentity.h"
#include "talk/base/gunit.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/transportdescription.h"
#include "talk/p2p/base/transportdescriptionfactory.h"

using talk_base::scoped_ptr;
using cricket::TransportDescriptionFactory;
using cricket::TransportDescription;
using cricket::TransportOptions;

// TODO(juberti): Change this to SHA-256 once we have Win32 using OpenSSL.
static const char* kDefaultDigestAlg = talk_base::DIGEST_SHA_1;

class TransportDescriptionFactoryTest : public testing::Test {
 public:
  TransportDescriptionFactoryTest()
      : id1_(new talk_base::FakeSSLIdentity("User1")),
        id2_(new talk_base::FakeSSLIdentity("User2")) {
    f1_.set_digest_algorithm(kDefaultDigestAlg);
    f2_.set_digest_algorithm(kDefaultDigestAlg);
  }
  void CheckDesc(const TransportDescription* desc, const std::string& type,
                 const std::string& opt, const std::string& ice_ufrag,
                 const std::string& ice_pwd, const std::string& dtls_alg) {
    ASSERT_TRUE(desc != NULL);
    EXPECT_EQ(type, desc->transport_type);
    EXPECT_EQ(!opt.empty(), desc->HasOption(opt));
    if (ice_ufrag.empty() && ice_pwd.empty()) {
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                desc->ice_ufrag.size());
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                desc->ice_pwd.size());
    } else {
      EXPECT_EQ(ice_ufrag, desc->ice_ufrag);
      EXPECT_EQ(ice_pwd, desc->ice_pwd);
    }
    if (dtls_alg.empty()) {
      EXPECT_TRUE(desc->identity_fingerprint.get() == NULL);
    } else {
      ASSERT_TRUE(desc->identity_fingerprint.get() != NULL);
      EXPECT_EQ(desc->identity_fingerprint->algorithm, dtls_alg);
      EXPECT_GT(desc->identity_fingerprint->digest.length(), 0U);
    }
  }

  // This test ice restart by doing two offer answer exchanges. On the second
  // exchange ice is restarted. The test verifies that the ufrag and password
  // in the offer and answer is changed.
  // If |dtls| is true, the test verifies that the finger print is not changed.
  void TestIceRestart(bool dtls) {
    if (dtls) {
      f1_.set_secure(cricket::SEC_ENABLED);
      f2_.set_secure(cricket::SEC_ENABLED);
      f1_.set_identity(id1_.get());
      f2_.set_identity(id2_.get());
    } else {
      f1_.set_secure(cricket::SEC_DISABLED);
      f2_.set_secure(cricket::SEC_DISABLED);
    }

    cricket::TransportOptions options;
    // The initial offer / answer exchange.
    talk_base::scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
        options, NULL));
    talk_base::scoped_ptr<TransportDescription> answer(
        f2_.CreateAnswer(offer.get(),
                         options, NULL));

    // Create an updated offer where we restart ice.
    options.ice_restart = true;
    talk_base::scoped_ptr<TransportDescription> restart_offer(f1_.CreateOffer(
        options, offer.get()));

    VerifyUfragAndPasswordChanged(dtls, offer.get(), restart_offer.get());

    // Create a new answer. The transport ufrag and password is changed since
    // |options.ice_restart == true|
    talk_base::scoped_ptr<TransportDescription> restart_answer(
        f2_.CreateAnswer(restart_offer.get(), options, answer.get()));
    ASSERT_TRUE(restart_answer.get() != NULL);

    VerifyUfragAndPasswordChanged(dtls, answer.get(), restart_answer.get());
  }

  void VerifyUfragAndPasswordChanged(bool dtls,
                                     const TransportDescription* org_desc,
                                     const TransportDescription* restart_desc) {
    EXPECT_NE(org_desc->ice_pwd, restart_desc->ice_pwd);
    EXPECT_NE(org_desc->ice_ufrag, restart_desc->ice_ufrag);
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
              restart_desc->ice_ufrag.size());
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
              restart_desc->ice_pwd.size());
    // If DTLS is enabled, make sure the finger print is unchanged.
    if (dtls) {
      EXPECT_FALSE(
          org_desc->identity_fingerprint->GetRfc4572Fingerprint().empty());
      EXPECT_EQ(org_desc->identity_fingerprint->GetRfc4572Fingerprint(),
                restart_desc->identity_fingerprint->GetRfc4572Fingerprint());
    }
  }

 protected:
  TransportDescriptionFactory f1_;
  TransportDescriptionFactory f2_;
  scoped_ptr<talk_base::SSLIdentity> id1_;
  scoped_ptr<talk_base::SSLIdentity> id2_;
};

// Test that in the default case, we generate the expected G-ICE offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test generating a hybrid offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "", "");
}

// Test generating an ICE-only offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test generating a hybrid offer with DTLS.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "",
            kDefaultDigestAlg);
  // Ensure it also works with SEC_REQUIRED.
  f1_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f1_.CreateOffer(TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "",
            kDefaultDigestAlg);
}

// Test generating a hybrid offer with DTLS fails with no identity.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtlsWithNoIdentity) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test generating a hybrid offer with DTLS fails with an unsupported digest.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtlsWithBadDigestAlg) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  f1_.set_digest_algorithm("bogus");
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test updating a hybrid offer with DTLS to pick ICE.
// The ICE credentials should stay the same in the new offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtlsReofferIceDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  scoped_ptr<TransportDescription> old_desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(old_desc.get() != NULL);
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> desc(
      f1_.CreateOffer(TransportOptions(), old_desc.get()));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "",
            old_desc->ice_ufrag, old_desc->ice_pwd, kDefaultDigestAlg);
}

// Test that we can answer a GICE offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test that we can answer a hybrid offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test that we can answer a hybrid offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we can answer an ICE offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we can't answer a GICE offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we can't answer an ICE offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we can update an answer properly; ICE credentials shouldn't change.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToIceReanswer) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> old_desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(old_desc.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(),
                       old_desc.get()));
  ASSERT_TRUE(desc.get() != NULL);
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "",
            old_desc->ice_ufrag, old_desc->ice_pwd, "");
}

// Test that we handle answering an offer with DTLS with no DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridToHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we handle answering an offer without DTLS if we have DTLS enabled,
// but fail if we require DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridDtlsToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_identity(id2_.get());
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we handle answering an DTLS offer with DTLS, both if we have
// DTLS enabled and required.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridDtlsToHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_identity(id2_.get());
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "",
            kDefaultDigestAlg);
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "",
            kDefaultDigestAlg);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true.
TEST_F(TransportDescriptionFactoryTest, TestIceRestart) {
  TestIceRestart(false);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true and DTLS is enabled.
TEST_F(TransportDescriptionFactoryTest, TestIceRestartWithDtls) {
  TestIceRestart(true);
}
