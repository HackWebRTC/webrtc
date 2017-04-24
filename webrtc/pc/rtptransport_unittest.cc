/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/fakepackettransport.h"
#include "webrtc/pc/rtptransport.h"

namespace webrtc {

class RtpTransportTest : public testing::Test {};

constexpr bool kMuxDisabled = false;
constexpr bool kMuxEnabled = true;

TEST_F(RtpTransportTest, SetRtcpParametersCantDisableRtcpMux) {
  RtpTransport transport(kMuxDisabled);
  RtcpParameters params;
  transport.SetRtcpParameters(params);
  params.mux = false;
  EXPECT_FALSE(transport.SetRtcpParameters(params).ok());
}

TEST_F(RtpTransportTest, SetRtcpParametersEmptyCnameUsesExisting) {
  static const char kName[] = "name";
  RtpTransport transport(kMuxDisabled);
  RtcpParameters params_with_name;
  params_with_name.cname = kName;
  transport.SetRtcpParameters(params_with_name);
  EXPECT_EQ(transport.GetRtcpParameters().cname, kName);

  RtcpParameters params_without_name;
  transport.SetRtcpParameters(params_without_name);
  EXPECT_EQ(transport.GetRtcpParameters().cname, kName);
}

class SignalObserver : public sigslot::has_slots<> {
 public:
  explicit SignalObserver(RtpTransport* transport) {
    transport->SignalReadyToSend.connect(this, &SignalObserver::OnReadyToSend);
  }
  void OnReadyToSend(bool ready) { ready_ = ready; }
  bool ready_ = false;
};

TEST_F(RtpTransportTest, SettingRtcpAndRtpSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtcp("fake_rtcp");
  fake_rtcp.SetWritable(true);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtcpPacketTransport(&fake_rtcp);  // rtcp ready
  EXPECT_FALSE(observer.ready_);
  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready_);
}

TEST_F(RtpTransportTest, SettingRtpAndRtcpSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtcp("fake_rtcp");
  fake_rtcp.SetWritable(true);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_FALSE(observer.ready_);
  transport.SetRtcpPacketTransport(&fake_rtcp);  // rtcp ready
  EXPECT_TRUE(observer.ready_);
}

TEST_F(RtpTransportTest, SettingRtpWithRtcpMuxEnabledSignalsReady) {
  RtpTransport transport(kMuxEnabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready_);
}

TEST_F(RtpTransportTest, DisablingRtcpMuxSignalsNotReady) {
  RtpTransport transport(kMuxEnabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready_);

  transport.SetRtcpMuxEnabled(false);
  EXPECT_FALSE(observer.ready_);
}

TEST_F(RtpTransportTest, EnablingRtcpMuxSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_FALSE(observer.ready_);

  transport.SetRtcpMuxEnabled(true);
  EXPECT_TRUE(observer.ready_);
}

class SignalCounter : public sigslot::has_slots<> {
 public:
  explicit SignalCounter(RtpTransport* transport) {
    transport->SignalReadyToSend.connect(this, &SignalCounter::OnReadyToSend);
  }
  void OnReadyToSend(bool ready) { ++count_; }
  int count_ = 0;
};

TEST_F(RtpTransportTest, ChangingReadyToSendStateOnlySignalsWhenChanged) {
  RtpTransport transport(kMuxEnabled);
  SignalCounter observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  // State changes, so we should signal.
  transport.SetRtpPacketTransport(&fake_rtp);
  EXPECT_EQ(observer.count_, 1);

  // State does not change, so we should not signal.
  transport.SetRtpPacketTransport(&fake_rtp);
  EXPECT_EQ(observer.count_, 1);

  // State does not change, so we should not signal.
  transport.SetRtcpMuxEnabled(true);
  EXPECT_EQ(observer.count_, 1);

  // State changes, so we should signal.
  transport.SetRtcpMuxEnabled(false);
  EXPECT_EQ(observer.count_, 2);
}

}  // namespace webrtc
