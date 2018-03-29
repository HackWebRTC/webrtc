/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "pc/jseptransport2.h"
#include "rtc_base/gunit.h"

namespace cricket {
using webrtc::SdpType;

static const char kIceUfrag1[] = "U001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIceUfrag2[] = "U002";
static const char kIcePwd2[] = "TESTIEPWD00000000000002";
static const char kTransportName[] = "Test Transport";

enum class SrtpMode {
  kSdes,
  kDtlsSrtp,
};

struct NegotiateRoleParams {
  ConnectionRole local_role;
  ConnectionRole remote_role;
  SdpType local_type;
  SdpType remote_type;
};

class JsepTransport2Test : public testing::Test, public sigslot::has_slots<> {
 protected:
  std::unique_ptr<webrtc::SrtpTransport> CreateSdesTransport(
      const std::string& transport_name,
      rtc::PacketTransportInternal* rtp_packet_transport,
      rtc::PacketTransportInternal* rtcp_packet_transport) {
    bool rtcp_mux_enabled = (rtcp_packet_transport == nullptr);
    auto srtp_transport =
        rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);

    srtp_transport->SetRtpPacketTransport(rtp_packet_transport);
    if (rtcp_packet_transport) {
      srtp_transport->SetRtcpPacketTransport(rtp_packet_transport);
    }
    return srtp_transport;
  }

  std::unique_ptr<webrtc::DtlsSrtpTransport> CreateDtlsSrtpTransport(
      const std::string& transport_name,
      cricket::DtlsTransportInternal* rtp_dtls_transport,
      cricket::DtlsTransportInternal* rtcp_dtls_transport) {
    bool rtcp_mux_enabled = (rtcp_dtls_transport == nullptr);
    auto srtp_transport =
        rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);
    auto dtls_srtp_transport =
        rtc::MakeUnique<webrtc::DtlsSrtpTransport>(std::move(srtp_transport));

    dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport,
                                           rtcp_dtls_transport);
    return dtls_srtp_transport;
  }

  // Create a new JsepTransport2 with a FakeDtlsTransport and a
  // FakeIceTransport.
  void CreateJsepTransport2(bool rtcp_mux_enabled, SrtpMode srtp_mode) {
    auto ice = rtc::MakeUnique<FakeIceTransport>(kTransportName,
                                                 ICE_CANDIDATE_COMPONENT_RTP);
    auto rtp_dtls_transport =
        rtc::MakeUnique<FakeDtlsTransport>(std::move(ice));

    std::unique_ptr<FakeDtlsTransport> rtcp_dtls_transport;
    if (!rtcp_mux_enabled) {
      ice = rtc::MakeUnique<FakeIceTransport>(kTransportName,
                                              ICE_CANDIDATE_COMPONENT_RTCP);
      rtcp_dtls_transport = rtc::MakeUnique<FakeDtlsTransport>(std::move(ice));
    }

    std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport;
    std::unique_ptr<webrtc::SrtpTransport> sdes_transport;
    std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport;
    switch (srtp_mode) {
      case SrtpMode::kSdes:
        sdes_transport =
            CreateSdesTransport(kTransportName, rtp_dtls_transport.get(),
                                rtcp_dtls_transport.get());
        sdes_transport_ = sdes_transport.get();
        break;
      case SrtpMode::kDtlsSrtp:
        dtls_srtp_transport =
            CreateDtlsSrtpTransport(kTransportName, rtp_dtls_transport.get(),
                                    rtcp_dtls_transport.get());
        break;
      default:
        RTC_NOTREACHED();
    }

    jsep_transport_ = rtc::MakeUnique<JsepTransport2>(
        kTransportName, /*local_certificate=*/nullptr,
        std::move(unencrypted_rtp_transport), std::move(sdes_transport),
        std::move(dtls_srtp_transport), std::move(rtp_dtls_transport),
        std::move(rtcp_dtls_transport));

    signal_rtcp_mux_active_received_ = false;
    jsep_transport_->SignalRtcpMuxActive.connect(
        this, &JsepTransport2Test::OnRtcpMuxActive);
  }

  JsepTransportDescription MakeJsepTransportDescription(
      bool rtcp_mux_enabled,
      const char* ufrag,
      const char* pwd,
      const rtc::scoped_refptr<rtc::RTCCertificate>& cert,
      ConnectionRole role = CONNECTIONROLE_NONE) {
    JsepTransportDescription jsep_description;
    jsep_description.rtcp_mux_enabled = rtcp_mux_enabled;

    std::unique_ptr<rtc::SSLFingerprint> fingerprint;
    if (cert) {
      fingerprint.reset(rtc::SSLFingerprint::CreateFromCertificate(cert));
    }
    jsep_description.transport_desc =
        TransportDescription(std::vector<std::string>(), ufrag, pwd,
                             ICEMODE_FULL, role, fingerprint.get());
    return jsep_description;
  }

  Candidate CreateCandidate(int component) {
    Candidate c;
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(component);
    c.set_protocol(UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

  void OnRtcpMuxActive() { signal_rtcp_mux_active_received_ = true; }

  std::unique_ptr<JsepTransport2> jsep_transport_;
  bool signal_rtcp_mux_active_received_ = false;
  // The SrtpTransport is owned by |jsep_transport_|. Keep a raw pointer here
  // for testing.
  webrtc::SrtpTransport* sdes_transport_ = nullptr;
};

// The parameterized tests cover both cases when RTCP mux is enable and
// disabled.
class JsepTransport2WithRtcpMux : public JsepTransport2Test,
                                  public testing::WithParamInterface<bool> {};

// This test verifies the ICE parameters are properly applied to the transports.
TEST_P(JsepTransport2WithRtcpMux, SetIceParameters) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  JsepTransportDescription jsep_description;
  jsep_description.transport_desc = TransportDescription(kIceUfrag1, kIcePwd1);
  jsep_description.rtcp_mux_enabled = rtcp_mux_enabled;
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(jsep_description, SdpType::kOffer)
          .ok());
  auto fake_ice_transport = static_cast<FakeIceTransport*>(
      jsep_transport_->rtp_dtls_transport()->ice_transport());
  EXPECT_EQ(ICEMODE_FULL, fake_ice_transport->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, fake_ice_transport->ice_ufrag());
  EXPECT_EQ(kIcePwd1, fake_ice_transport->ice_pwd());
  if (!rtcp_mux_enabled) {
    fake_ice_transport = static_cast<FakeIceTransport*>(
        jsep_transport_->rtcp_dtls_transport()->ice_transport());
    ASSERT_TRUE(fake_ice_transport);
    EXPECT_EQ(ICEMODE_FULL, fake_ice_transport->remote_ice_mode());
    EXPECT_EQ(kIceUfrag1, fake_ice_transport->ice_ufrag());
    EXPECT_EQ(kIcePwd1, fake_ice_transport->ice_pwd());
  }

  jsep_description.transport_desc = TransportDescription(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(jsep_transport_
                  ->SetRemoteJsepTransportDescription(jsep_description,
                                                      SdpType::kAnswer)
                  .ok());
  fake_ice_transport = static_cast<FakeIceTransport*>(
      jsep_transport_->rtp_dtls_transport()->ice_transport());
  EXPECT_EQ(ICEMODE_FULL, fake_ice_transport->remote_ice_mode());
  EXPECT_EQ(kIceUfrag2, fake_ice_transport->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd2, fake_ice_transport->remote_ice_pwd());
  if (!rtcp_mux_enabled) {
    fake_ice_transport = static_cast<FakeIceTransport*>(
        jsep_transport_->rtcp_dtls_transport()->ice_transport());
    ASSERT_TRUE(fake_ice_transport);
    EXPECT_EQ(ICEMODE_FULL, fake_ice_transport->remote_ice_mode());
    EXPECT_EQ(kIceUfrag2, fake_ice_transport->remote_ice_ufrag());
    EXPECT_EQ(kIcePwd2, fake_ice_transport->remote_ice_pwd());
  }
}

// Similarly, test DTLS parameters are properly applied to the transports.
TEST_P(JsepTransport2WithRtcpMux, SetDtlsParameters) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  // Create certificates.
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("local", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> remote_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("remote", rtc::KT_DEFAULT)));
  jsep_transport_->SetLocalCertificate(local_cert);

  // Apply offer.
  JsepTransportDescription local_description =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   local_cert, CONNECTIONROLE_ACTPASS);
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_description, SdpType::kOffer)
          .ok());
  // Apply Answer.
  JsepTransportDescription remote_description =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   remote_cert, CONNECTIONROLE_ACTIVE);
  ASSERT_TRUE(jsep_transport_
                  ->SetRemoteJsepTransportDescription(remote_description,
                                                      SdpType::kAnswer)
                  .ok());

  // Verify that SSL role and remote fingerprint were set correctly based on
  // transport descriptions.
  auto role = jsep_transport_->GetDtlsRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_SERVER, role);  // Because remote description was "active".
  auto fake_dtls =
      static_cast<FakeDtlsTransport*>(jsep_transport_->rtp_dtls_transport());
  EXPECT_EQ(remote_description.transport_desc.identity_fingerprint->ToString(),
            fake_dtls->dtls_fingerprint().ToString());

  if (!rtcp_mux_enabled) {
    auto fake_rtcp_dtls =
        static_cast<FakeDtlsTransport*>(jsep_transport_->rtcp_dtls_transport());
    EXPECT_EQ(
        remote_description.transport_desc.identity_fingerprint->ToString(),
        fake_rtcp_dtls->dtls_fingerprint().ToString());
  }
}

// Same as above test, but with remote transport description using
// CONNECTIONROLE_PASSIVE, expecting SSL_CLIENT role.
TEST_P(JsepTransport2WithRtcpMux, SetDtlsParametersWithPassiveAnswer) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  // Create certificates.
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("local", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> remote_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("remote", rtc::KT_DEFAULT)));
  jsep_transport_->SetLocalCertificate(local_cert);

  // Apply offer.
  JsepTransportDescription local_description =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   local_cert, CONNECTIONROLE_ACTPASS);
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_description, SdpType::kOffer)
          .ok());
  // Apply Answer.
  JsepTransportDescription remote_description =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   remote_cert, CONNECTIONROLE_PASSIVE);
  ASSERT_TRUE(jsep_transport_
                  ->SetRemoteJsepTransportDescription(remote_description,
                                                      SdpType::kAnswer)
                  .ok());

  // Verify that SSL role and remote fingerprint were set correctly based on
  // transport descriptions.
  auto role = jsep_transport_->GetDtlsRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT,
            role);  // Because remote description was "passive".
  auto fake_dtls =
      static_cast<FakeDtlsTransport*>(jsep_transport_->rtp_dtls_transport());
  EXPECT_EQ(remote_description.transport_desc.identity_fingerprint->ToString(),
            fake_dtls->dtls_fingerprint().ToString());

  if (!rtcp_mux_enabled) {
    auto fake_rtcp_dtls =
        static_cast<FakeDtlsTransport*>(jsep_transport_->rtcp_dtls_transport());
    EXPECT_EQ(
        remote_description.transport_desc.identity_fingerprint->ToString(),
        fake_rtcp_dtls->dtls_fingerprint().ToString());
  }
}

// Tests SetNeedsIceRestartFlag and need_ice_restart, ensuring needs_ice_restart
// only starts returning "false" once an ICE restart has been initiated.
TEST_P(JsepTransport2WithRtcpMux, NeedsIceRestart) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  // Use the same JsepTransportDescription for both offer and answer.
  JsepTransportDescription description;
  description.transport_desc = TransportDescription(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(description, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(description, SdpType::kAnswer)
          .ok());
  // Flag initially should be false.
  EXPECT_FALSE(jsep_transport_->needs_ice_restart());

  // After setting flag, it should be true.
  jsep_transport_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(jsep_transport_->needs_ice_restart());

  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(description, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(description, SdpType::kAnswer)
          .ok());
  EXPECT_TRUE(jsep_transport_->needs_ice_restart());

  // Doing an offer/answer that restarts ICE should clear the flag.
  description.transport_desc = TransportDescription(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(description, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(description, SdpType::kAnswer)
          .ok());
  EXPECT_FALSE(jsep_transport_->needs_ice_restart());
}

TEST_P(JsepTransport2WithRtcpMux, GetStats) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  size_t expected_stats_size = rtcp_mux_enabled ? 1u : 2u;
  TransportStats stats;
  EXPECT_TRUE(jsep_transport_->GetStats(&stats));
  EXPECT_EQ(expected_stats_size, stats.channel_stats.size());
  EXPECT_EQ(ICE_CANDIDATE_COMPONENT_RTP, stats.channel_stats[0].component);
  if (!rtcp_mux_enabled) {
    EXPECT_EQ(ICE_CANDIDATE_COMPONENT_RTCP, stats.channel_stats[1].component);
  }
}

// Tests that VerifyCertificateFingerprint only returns true when the
// certificate matches the fingerprint.
TEST_P(JsepTransport2WithRtcpMux, VerifyCertificateFingerprint) {
  bool rtcp_mux_enabled = GetParam();
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);

  EXPECT_FALSE(
      jsep_transport_->VerifyCertificateFingerprint(nullptr, nullptr).ok());
  rtc::KeyType key_types[] = {rtc::KT_RSA, rtc::KT_ECDSA};

  for (auto& key_type : key_types) {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate("testing", key_type)));
    ASSERT_NE(nullptr, certificate);

    std::string digest_algorithm;
    ASSERT_TRUE(certificate->ssl_certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_FALSE(digest_algorithm.empty());
    std::unique_ptr<rtc::SSLFingerprint> good_fingerprint(
        rtc::SSLFingerprint::Create(digest_algorithm, certificate->identity()));
    ASSERT_NE(nullptr, good_fingerprint);

    EXPECT_TRUE(jsep_transport_
                    ->VerifyCertificateFingerprint(certificate.get(),
                                                   good_fingerprint.get())
                    .ok());
    EXPECT_FALSE(jsep_transport_
                     ->VerifyCertificateFingerprint(certificate.get(), nullptr)
                     .ok());
    EXPECT_FALSE(
        jsep_transport_
            ->VerifyCertificateFingerprint(nullptr, good_fingerprint.get())
            .ok());

    rtc::SSLFingerprint bad_fingerprint = *good_fingerprint;
    bad_fingerprint.digest.AppendData("0", 1);
    EXPECT_FALSE(
        jsep_transport_
            ->VerifyCertificateFingerprint(certificate.get(), &bad_fingerprint)
            .ok());
  }
}

// Tests the logic of DTLS role negotiation for an initial offer/answer.
TEST_P(JsepTransport2WithRtcpMux, ValidDtlsRoleNegotiation) {
  bool rtcp_mux_enabled = GetParam();
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));

  JsepTransportDescription local_description = MakeJsepTransportDescription(
      rtcp_mux_enabled, kIceUfrag1, kIcePwd1, certificate);
  JsepTransportDescription remote_description = MakeJsepTransportDescription(
      rtcp_mux_enabled, kIceUfrag2, kIcePwd2, certificate);

  // Parameters which set the SSL role to SSL_CLIENT.
  NegotiateRoleParams valid_client_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : valid_client_params) {
    CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
    jsep_transport_->SetLocalCertificate(certificate);

    local_description.transport_desc.connection_role = param.local_role;
    remote_description.transport_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
    } else {
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
    }
    EXPECT_EQ(rtc::SSL_CLIENT, *jsep_transport_->GetDtlsRole());
  }

  // Parameters which set the SSL role to SSL_SERVER.
  NegotiateRoleParams valid_server_params[] = {
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : valid_server_params) {
    CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
    jsep_transport_->SetLocalCertificate(certificate);

    local_description.transport_desc.connection_role = param.local_role;
    remote_description.transport_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
    } else {
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
    }
    EXPECT_EQ(rtc::SSL_SERVER, *jsep_transport_->GetDtlsRole());
  }
}

// Tests the logic of DTLS role negotiation for an initial offer/answer.
TEST_P(JsepTransport2WithRtcpMux, InvalidDtlsRoleNegotiation) {
  bool rtcp_mux_enabled = GetParam();
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));

  JsepTransportDescription local_description = MakeJsepTransportDescription(
      rtcp_mux_enabled, kIceUfrag1, kIcePwd1, certificate);
  JsepTransportDescription remote_description = MakeJsepTransportDescription(
      rtcp_mux_enabled, kIceUfrag2, kIcePwd2, certificate);

  NegotiateRoleParams duplicate_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : duplicate_params) {
    CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
    jsep_transport_->SetLocalCertificate(certificate);

    local_description.transport_desc.connection_role = param.local_role;
    remote_description.transport_desc.connection_role = param.remote_role;

    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
      EXPECT_FALSE(jsep_transport_
                       ->SetRemoteJsepTransportDescription(remote_description,
                                                           param.remote_type)
                       .ok());
    } else {
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
      EXPECT_FALSE(jsep_transport_
                       ->SetLocalJsepTransportDescription(local_description,
                                                          param.local_type)
                       .ok());
    }
  }

  // Invalid parameters due to the offerer not using ACTPASS.
  NegotiateRoleParams offerer_without_actpass_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : offerer_without_actpass_params) {
    CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
    jsep_transport_->SetLocalCertificate(certificate);

    local_description.transport_desc.connection_role = param.local_role;
    remote_description.transport_desc.connection_role = param.remote_role;

    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(jsep_transport_
                      ->SetLocalJsepTransportDescription(local_description,
                                                         param.local_type)
                      .ok());
      EXPECT_FALSE(jsep_transport_
                       ->SetRemoteJsepTransportDescription(remote_description,
                                                           param.remote_type)
                       .ok());
    } else {
      EXPECT_TRUE(jsep_transport_
                      ->SetRemoteJsepTransportDescription(remote_description,
                                                          param.remote_type)
                      .ok());
      EXPECT_FALSE(jsep_transport_
                       ->SetLocalJsepTransportDescription(local_description,
                                                          param.local_type)
                       .ok());
    }
  }
}

INSTANTIATE_TEST_CASE_P(JsepTransport2Test,
                        JsepTransport2WithRtcpMux,
                        testing::Bool());

// Test that a reoffer in the opposite direction is successful as long as the
// role isn't changing. Doesn't test every possible combination like the test
// above.
TEST_F(JsepTransport2Test, ValidDtlsReofferFromAnswerer) {
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  bool rtcp_mux_enabled = true;
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
  jsep_transport_->SetLocalCertificate(certificate);

  JsepTransportDescription local_offer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription remote_answer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_offer, SdpType::kOffer)
          .ok());
  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_answer, SdpType::kAnswer)
          .ok());

  // We were actpass->active previously, now in the other direction it's
  // actpass->passive.
  JsepTransportDescription remote_offer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription local_answer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_PASSIVE);

  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_offer, SdpType::kOffer)
          .ok());
  EXPECT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_answer, SdpType::kAnswer)
          .ok());
}

// Test that a reoffer in the opposite direction fails if the role changes.
// Inverse of test above.
TEST_F(JsepTransport2Test, InvalidDtlsReofferFromAnswerer) {
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  bool rtcp_mux_enabled = true;
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
  jsep_transport_->SetLocalCertificate(certificate);

  JsepTransportDescription local_offer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription remote_answer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_offer, SdpType::kOffer)
          .ok());
  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_answer, SdpType::kAnswer)
          .ok());

  // Changing role to passive here isn't allowed. Though for some reason this
  // only fails in SetLocalTransportDescription.
  JsepTransportDescription remote_offer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_PASSIVE);
  JsepTransportDescription local_answer =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_offer, SdpType::kOffer)
          .ok());
  EXPECT_FALSE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_answer, SdpType::kAnswer)
          .ok());
}

// Test that a remote offer with the current negotiated role can be accepted.
// This is allowed by dtls-sdp, though we'll never generate such an offer,
// since JSEP requires generating "actpass".
TEST_F(JsepTransport2Test, RemoteOfferWithCurrentNegotiatedDtlsRole) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  bool rtcp_mux_enabled = true;
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
  jsep_transport_->SetLocalCertificate(certificate);

  JsepTransportDescription remote_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription local_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTIVE);

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kAnswer)
          .ok());

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = jsep_transport_->GetDtlsRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with current negotiated role of "passive".
  remote_desc.transport_desc.connection_role = CONNECTIONROLE_PASSIVE;
  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kOffer)
          .ok());
  EXPECT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kAnswer)
          .ok());
}

// Test that a remote offer with the inverse of the current negotiated DTLS
// role is rejected.
TEST_F(JsepTransport2Test, RemoteOfferThatChangesNegotiatedDtlsRole) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  bool rtcp_mux_enabled = true;
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
  jsep_transport_->SetLocalCertificate(certificate);

  JsepTransportDescription remote_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription local_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTIVE);

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kAnswer)
          .ok());

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = jsep_transport_->GetDtlsRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with current negotiated role of "passive".
  remote_desc.transport_desc.connection_role = CONNECTIONROLE_ACTIVE;
  EXPECT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kOffer)
          .ok());
  EXPECT_FALSE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kAnswer)
          .ok());
}

// Testing that a legacy client that doesn't use the setup attribute will be
// interpreted as having an active role.
TEST_F(JsepTransport2Test, DtlsSetupWithLegacyAsAnswerer) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  bool rtcp_mux_enabled = true;
  CreateJsepTransport2(rtcp_mux_enabled, SrtpMode::kDtlsSrtp);
  jsep_transport_->SetLocalCertificate(certificate);

  JsepTransportDescription remote_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag1, kIcePwd1,
                                   certificate, CONNECTIONROLE_ACTPASS);
  JsepTransportDescription local_desc =
      MakeJsepTransportDescription(rtcp_mux_enabled, kIceUfrag2, kIcePwd2,
                                   certificate, CONNECTIONROLE_ACTIVE);

  local_desc.transport_desc.connection_role = CONNECTIONROLE_ACTPASS;
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kOffer)
          .ok());
  // Use CONNECTIONROLE_NONE to simulate legacy endpoint.
  remote_desc.transport_desc.connection_role = CONNECTIONROLE_NONE;
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kAnswer)
          .ok());

  rtc::Optional<rtc::SSLRole> role = jsep_transport_->GetDtlsRole();
  ASSERT_TRUE(role);
  // Since legacy answer ommitted setup atribute, and we offered actpass, we
  // should act as passive (server).
  EXPECT_EQ(rtc::SSL_SERVER, *role);
}

// Tests that when the RTCP mux is successfully negotiated, the RTCP transport
// will be destroyed and the SignalRtpMuxActive will be fired.
TEST_F(JsepTransport2Test, RtcpMuxNegotiation) {
  CreateJsepTransport2(/*rtcp_mux_enabled=*/false, SrtpMode::kDtlsSrtp);
  JsepTransportDescription local_desc;
  local_desc.rtcp_mux_enabled = true;
  EXPECT_NE(nullptr, jsep_transport_->rtcp_dtls_transport());
  EXPECT_FALSE(signal_rtcp_mux_active_received_);

  // The remote side supports RTCP-mux.
  JsepTransportDescription remote_desc;
  remote_desc.rtcp_mux_enabled = true;
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kAnswer)
          .ok());

  EXPECT_EQ(nullptr, jsep_transport_->rtcp_dtls_transport());
  EXPECT_TRUE(signal_rtcp_mux_active_received_);

  // The remote side doesn't support RTCP-mux.
  CreateJsepTransport2(/*rtcp_mux_enabled=*/false, SrtpMode::kDtlsSrtp);
  signal_rtcp_mux_active_received_ = false;
  remote_desc.rtcp_mux_enabled = false;
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(local_desc, SdpType::kOffer)
          .ok());
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(remote_desc, SdpType::kAnswer)
          .ok());

  EXPECT_NE(nullptr, jsep_transport_->rtcp_dtls_transport());
  EXPECT_FALSE(signal_rtcp_mux_active_received_);
}

TEST_F(JsepTransport2Test, SdesNegotiation) {
  CreateJsepTransport2(/*rtcp_mux_enabled=*/true, SrtpMode::kSdes);
  ASSERT_TRUE(sdes_transport_);
  EXPECT_FALSE(sdes_transport_->IsActive());

  JsepTransportDescription offer_desc;
  offer_desc.cryptos.push_back(cricket::CryptoParams(
      1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
      "inline:" + rtc::CreateRandomString(40), std::string()));
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(offer_desc, SdpType::kOffer)
          .ok());

  JsepTransportDescription answer_desc;
  answer_desc.cryptos.push_back(cricket::CryptoParams(
      1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
      "inline:" + rtc::CreateRandomString(40), std::string()));
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(answer_desc, SdpType::kAnswer)
          .ok());
  EXPECT_TRUE(sdes_transport_->IsActive());
}

TEST_F(JsepTransport2Test, SdesNegotiationWithEmptyCryptosInAnswer) {
  CreateJsepTransport2(/*rtcp_mux_enabled=*/true, SrtpMode::kSdes);
  ASSERT_TRUE(sdes_transport_);
  EXPECT_FALSE(sdes_transport_->IsActive());

  JsepTransportDescription offer_desc;
  offer_desc.cryptos.push_back(cricket::CryptoParams(
      1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
      "inline:" + rtc::CreateRandomString(40), std::string()));
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(offer_desc, SdpType::kOffer)
          .ok());

  JsepTransportDescription answer_desc;
  ASSERT_TRUE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(answer_desc, SdpType::kAnswer)
          .ok());
  // SRTP is not active because the crypto parameter is answer is empty.
  EXPECT_FALSE(sdes_transport_->IsActive());
}

TEST_F(JsepTransport2Test, SdesNegotiationWithMismatchedCryptos) {
  CreateJsepTransport2(/*rtcp_mux_enabled=*/true, SrtpMode::kSdes);
  ASSERT_TRUE(sdes_transport_);
  EXPECT_FALSE(sdes_transport_->IsActive());

  JsepTransportDescription offer_desc;
  offer_desc.cryptos.push_back(cricket::CryptoParams(
      1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
      "inline:" + rtc::CreateRandomString(40), std::string()));
  ASSERT_TRUE(
      jsep_transport_
          ->SetLocalJsepTransportDescription(offer_desc, SdpType::kOffer)
          .ok());

  JsepTransportDescription answer_desc;
  answer_desc.cryptos.push_back(cricket::CryptoParams(
      1, rtc::CS_AES_CM_128_HMAC_SHA1_80,
      "inline:" + rtc::CreateRandomString(40), std::string()));
  // Expected to fail because the crypto parameters don't match.
  ASSERT_FALSE(
      jsep_transport_
          ->SetRemoteJsepTransportDescription(answer_desc, SdpType::kAnswer)
          .ok());
}

// Tests that the remote candidates can be added to the transports after both
// local and remote descriptions are set.
TEST_F(JsepTransport2Test, AddRemoteCandidates) {
  CreateJsepTransport2(/*rtcp_mux_enabled=*/true, SrtpMode::kDtlsSrtp);
  auto fake_ice_transport = static_cast<FakeIceTransport*>(
      jsep_transport_->rtp_dtls_transport()->ice_transport());

  Candidates candidates;
  candidates.push_back(CreateCandidate(/*COMPONENT_RTP*/ 1));
  candidates.push_back(CreateCandidate(/*COMPONENT_RTP*/ 1));

  JsepTransportDescription desc;
  ASSERT_TRUE(
      jsep_transport_->SetLocalJsepTransportDescription(desc, SdpType::kOffer)
          .ok());
  // Expected to fail because the remote description is unset.
  EXPECT_FALSE(jsep_transport_->AddRemoteCandidates(candidates).ok());

  ASSERT_TRUE(
      jsep_transport_->SetRemoteJsepTransportDescription(desc, SdpType::kAnswer)
          .ok());
  EXPECT_EQ(0u, fake_ice_transport->remote_candidates().size());
  EXPECT_TRUE(jsep_transport_->AddRemoteCandidates(candidates).ok());
  EXPECT_EQ(candidates.size(), fake_ice_transport->remote_candidates().size());
}

}  // namespace cricket
