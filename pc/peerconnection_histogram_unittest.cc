/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tuple>

#include "absl/memory/memory.h"
#include "api/peerconnectionproxy.h"
#include "media/base/fakemediaengine.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtualsocketserver.h"
#include "system_wrappers/include/metrics_default.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;

static constexpr int kDefaultTimeout = 10000;

int MakeUsageFingerprint(std::set<PeerConnection::UsageEvent> events) {
  int signature = 0;
  for (const auto it : events) {
    signature |= static_cast<int>(it);
  }
  return signature;
}

class PeerConnectionFactoryForUsageHistogramTest
    : public rtc::RefCountedObject<PeerConnectionFactory> {
 public:
  PeerConnectionFactoryForUsageHistogramTest()
      : rtc::RefCountedObject<PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            absl::make_unique<cricket::FakeMediaEngine>(),
            CreateCallFactory(),
            nullptr) {}

  void ActionsBeforeInitializeForTesting(PeerConnectionInterface* pc) override {
    PeerConnection* internal_pc = static_cast<PeerConnection*>(pc);
    if (return_histogram_very_quickly_) {
      internal_pc->ReturnHistogramVeryQuicklyForTesting();
    }
  }

  void ReturnHistogramVeryQuickly() { return_histogram_very_quickly_ = true; }

 private:
  bool return_histogram_very_quickly_ = false;
};

class PeerConnectionWrapperForUsageHistogramTest;
typedef PeerConnectionWrapperForUsageHistogramTest* RawWrapperPtr;

class ObserverForUsageHistogramTest : public MockPeerConnectionObserver {
 public:
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void PrepareToExchangeCandidates(RawWrapperPtr other) {
    candidate_target_ = other;
  }

  bool HaveDataChannel() { return last_datachannel_; }

 private:
  RawWrapperPtr candidate_target_;  // Note: Not thread-safe against deletions.
};

class PeerConnectionWrapperForUsageHistogramTest
    : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  PeerConnection* GetInternalPeerConnection() {
    auto* pci =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc());
    return static_cast<PeerConnection*>(pci->internal());
  }

  void PrepareToExchangeCandidates(
      PeerConnectionWrapperForUsageHistogramTest* other) {
    static_cast<ObserverForUsageHistogramTest*>(observer())
        ->PrepareToExchangeCandidates(other);
    static_cast<ObserverForUsageHistogramTest*>(other->observer())
        ->PrepareToExchangeCandidates(this);
  }

  bool IsConnected() {
    return pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionConnected ||
           pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionCompleted;
  }

  bool HaveDataChannel() {
    return static_cast<ObserverForUsageHistogramTest*>(observer())
        ->HaveDataChannel();
  }
};

void ObserverForUsageHistogramTest::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  if (candidate_target_) {
    candidate_target_->pc()->AddIceCandidate(candidate);
  }
}

class PeerConnectionUsageHistogramTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForUsageHistogramTest>
      WrapperPtr;

  PeerConnectionUsageHistogramTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
    webrtc::metrics::Reset();
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(
        RTCConfiguration(), PeerConnectionFactoryInterface::Options(), false);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(
        config, PeerConnectionFactoryInterface::Options(), false);
  }

  WrapperPtr CreatePeerConnectionWithImmediateReport() {
    return CreatePeerConnection(
        RTCConfiguration(), PeerConnectionFactoryInterface::Options(), true);
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      const PeerConnectionFactoryInterface::Options factory_options,
      bool immediate_report) {
    rtc::scoped_refptr<PeerConnectionFactoryForUsageHistogramTest> pc_factory(
        new PeerConnectionFactoryForUsageHistogramTest());
    pc_factory->SetOptions(factory_options);
    RTC_CHECK(pc_factory->Initialize());
    if (immediate_report) {
      pc_factory->ReturnHistogramVeryQuickly();
    }
    auto observer = absl::make_unique<ObserverForUsageHistogramTest>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper =
        absl::make_unique<PeerConnectionWrapperForUsageHistogramTest>(
            pc_factory, pc, std::move(observer));
    return wrapper;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionUsageHistogramTest, UsageFingerprintHistogramFromTimeout) {
  auto pc = CreatePeerConnectionWithImmediateReport();

  int expected_fingerprint = MakeUsageFingerprint({});
  ASSERT_TRUE_WAIT(
      1u == webrtc::metrics::NumSamples("WebRTC.PeerConnection.UsagePattern"),
      kDefaultTimeout);
  EXPECT_EQ(1, webrtc::metrics::NumEvents("WebRTC.PeerConnection.UsagePattern",
                                          expected_fingerprint));
}

#ifndef WEBRTC_ANDROID
// These tests do not work on Android. Why is unclear.
// https://bugs.webrtc.org/9461

// Test getting the usage fingerprint for an audio/video connection.
TEST_F(PeerConnectionUsageHistogramTest, FingerprintAudioVideo) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  caller->PrepareToExchangeCandidates(callee.get());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  ASSERT_TRUE_WAIT(caller->IsConnected(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee->IsConnected(), kDefaultTimeout);
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::AUDIO_ADDED,
       PeerConnection::UsageEvent::VIDEO_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::REMOTE_CANDIDATE_ADDED,
       PeerConnection::UsageEvent::ICE_STATE_CONNECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(2,
            webrtc::metrics::NumSamples("WebRTC.PeerConnection.UsagePattern"));
  EXPECT_EQ(2, webrtc::metrics::NumEvents("WebRTC.PeerConnection.UsagePattern",
                                          expected_fingerprint));
}

#ifdef HAVE_SCTP
TEST_F(PeerConnectionUsageHistogramTest, FingerprintDataOnly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("foodata");
  caller->PrepareToExchangeCandidates(callee.get());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  ASSERT_TRUE_WAIT(callee->HaveDataChannel(), kDefaultTimeout);
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::DATA_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::REMOTE_CANDIDATE_ADDED,
       PeerConnection::UsageEvent::ICE_STATE_CONNECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(2,
            webrtc::metrics::NumSamples("WebRTC.PeerConnection.UsagePattern"));
  EXPECT_EQ(2, webrtc::metrics::NumEvents("WebRTC.PeerConnection.UsagePattern",
                                          expected_fingerprint));
}
#endif  // HAVE_SCTP
#endif  // WEBRTC_ANDROID

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurn) {
  RTCConfiguration configuration;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server/"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server/"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection(configuration);
  ASSERT_TRUE(caller);
  caller->pc()->Close();
  int expected_fingerprint =
      MakeUsageFingerprint({PeerConnection::UsageEvent::STUN_SERVER_ADDED,
                            PeerConnection::UsageEvent::TURN_SERVER_ADDED,
                            PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(1,
            webrtc::metrics::NumSamples("WebRTC.PeerConnection.UsagePattern"));
  EXPECT_EQ(1, webrtc::metrics::NumEvents("WebRTC.PeerConnection.UsagePattern",
                                          expected_fingerprint));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurnInReconfiguration) {
  RTCConfiguration configuration;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server/"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server/"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection();
  ASSERT_TRUE(caller);
  RTCError error;
  caller->pc()->SetConfiguration(configuration, &error);
  ASSERT_TRUE(error.ok());
  caller->pc()->Close();
  int expected_fingerprint =
      MakeUsageFingerprint({PeerConnection::UsageEvent::STUN_SERVER_ADDED,
                            PeerConnection::UsageEvent::TURN_SERVER_ADDED,
                            PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(1,
            webrtc::metrics::NumSamples("WebRTC.PeerConnection.UsagePattern"));
  EXPECT_EQ(1, webrtc::metrics::NumEvents("WebRTC.PeerConnection.UsagePattern",
                                          expected_fingerprint));
}

}  // namespace webrtc
