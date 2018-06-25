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

#include "api/fakemetricsobserver.h"
#include "api/peerconnectionproxy.h"
#include "media/base/fakemediaengine.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"

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
            rtc::MakeUnique<cricket::FakeMediaEngine>(),
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
  bool return_histogram_very_quickly_;
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
};

class PeerConnectionUsageHistogramTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForUsageHistogramTest>
      WrapperPtr;

  PeerConnectionUsageHistogramTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(
        RTCConfiguration(), PeerConnectionFactoryInterface::Options(), false);
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
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForUsageHistogramTest>(
        pc_factory, pc, std::move(observer));
    return wrapper;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionUsageHistogramTest, UsageFingerprintHistogramFromTimeout) {
  auto pc = CreatePeerConnectionWithImmediateReport();

  // Register UMA observer before signaling begins.
  rtc::scoped_refptr<webrtc::FakeMetricsObserver> caller_observer =
      new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  pc->GetInternalPeerConnection()->RegisterUMAObserver(caller_observer);
  int expected_fingerprint = MakeUsageFingerprint({});
  ASSERT_TRUE_WAIT(caller_observer->ExpectOnlySingleEnumCount(
                       webrtc::kEnumCounterUsagePattern, expected_fingerprint),
                   kDefaultTimeout);
}

}  // namespace webrtc
