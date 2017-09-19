/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtptransport.h"

#include "pc/rtptransport.h"
#include "pc/rtptransporttestutil.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "test/gmock.h"

namespace webrtc {

using testing::_;
using testing::Return;

class MockRtpTransport : public RtpTransport {
 public:
  MockRtpTransport() : RtpTransport(true) {}

  MOCK_METHOD4(SendPacket,
               bool(bool rtcp,
                    rtc::CopyOnWriteBuffer* packet,
                    const rtc::PacketOptions& options,
                    int flags));

  void PretendReceivedPacket() {
    bool rtcp = false;
    rtc::CopyOnWriteBuffer buffer;
    rtc::PacketTime time;
    SignalPacketReceived(rtcp, &buffer, time);
  }
};

TEST(SrtpTransportTest, SendPacket) {
  auto rtp_transport = rtc::MakeUnique<MockRtpTransport>();
  EXPECT_CALL(*rtp_transport, SendPacket(_, _, _, _)).WillOnce(Return(true));

  SrtpTransport srtp_transport(std::move(rtp_transport), "a");

  const bool rtcp = false;
  rtc::CopyOnWriteBuffer packet;
  rtc::PacketOptions options;
  int flags = 0;
  EXPECT_TRUE(srtp_transport.SendPacket(rtcp, &packet, options, flags));

  // TODO(zstein): Also verify that the packet received by RtpTransport has been
  // protected once SrtpTransport handles that.
}

// Test that SrtpTransport fires SignalPacketReceived when the underlying
// RtpTransport fires SignalPacketReceived.
TEST(SrtpTransportTest, SignalPacketReceived) {
  auto rtp_transport = rtc::MakeUnique<MockRtpTransport>();
  MockRtpTransport* rtp_transport_raw = rtp_transport.get();
  SrtpTransport srtp_transport(std::move(rtp_transport), "a");

  SignalPacketReceivedCounter counter(&srtp_transport);

  rtp_transport_raw->PretendReceivedPacket();

  EXPECT_EQ(1, counter.rtp_count());

  // TODO(zstein): Also verify that the packet is unprotected once SrtpTransport
  // handles that.
}

}  // namespace webrtc
