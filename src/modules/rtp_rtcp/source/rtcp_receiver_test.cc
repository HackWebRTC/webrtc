/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RTCPReceiver.
 */
#include <gtest/gtest.h>

// Note: This file has no directory. Lint warning must be ignored.
#include "common_types.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "modules/rtp_rtcp/source/rtcp_sender.h"
#include "modules/rtp_rtcp/source/rtcp_receiver.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl.h"

namespace webrtc {

namespace {  // Anonymous namespace; hide utility functions and classes.

// This test transport verifies that no functions get called.
class TestTransport : public Transport,
                      public RtpData {
 public:
  explicit TestTransport(RTCPReceiver* rtcp_receiver) :
    rtcp_receiver_(rtcp_receiver) {
  }

  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    ADD_FAILURE();  // FAIL() gives a compile error.
    return -1;
  }

  // Injects an RTCP packet into the receiver.
  virtual int SendRTCPPacket(int /* ch */, const void *packet, int packet_len) {
    ADD_FAILURE();
    return 0;
  }

  virtual int OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadSize,
                                    const WebRtcRTPHeader* rtpHeader) {
    ADD_FAILURE();
    return 0;
  }
  RTCPReceiver* rtcp_receiver_;
};

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest() {
    system_clock_ = ModuleRTPUtility::GetSystemClock();
    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(0, false, system_clock_);
    rtcp_receiver_ = new RTCPReceiver(0, system_clock_, rtp_rtcp_impl_);
    test_transport_ = new TestTransport(rtcp_receiver_);
    EXPECT_EQ(0, rtp_rtcp_impl_->RegisterIncomingDataCallback(test_transport_));
  }
  ~RtcpReceiverTest() {
    delete rtcp_sender_;
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
    delete system_clock_;
  }

  // Injects an RTCP packet into the receiver.
  // Returns 0 for OK, non-0 for failure.
  int InjectRtcpPacket(const WebRtc_UWord8* packet,
                        WebRtc_UWord16 packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser(packet,
                                         packet_len,
                                         true);  // Allow non-compound RTCP

    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    int result = rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser);
    rtcp_packet_info_ = rtcpPacketInformation;
    return result;
  }

  RtpRtcpClock* system_clock_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
};


TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const WebRtc_UWord8 bad_packet[] = {0, 0, 0, 0};
  EXPECT_EQ(0, InjectRtcpPacket(bad_packet, sizeof(bad_packet)));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSrPacket) {
  const WebRtc_UWord8 sr_packet[] = {
    0x81, 200,  // Type 200, report count = 0
    0, 6,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 1, 2, 3, 4, 5, 6, 7,  // NTP timestamp
    0, 1, 2, 3,  // RTP timestamp
    0, 0, 0, 0,  // Sender's packet count
    0, 0, 0, 0  // Sender's octet count
  };
  EXPECT_EQ(0, InjectRtcpPacket(sr_packet, sizeof(sr_packet)));
  // The parser will note the remote SSRC on a SR from other than his
  // expected peer, but will not flag that he's gotten a packet.
  EXPECT_EQ(0x010203U, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(0U,
            kRtcpSr & rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, TmmbrReceivedWithNoIncomingPacket) {
  // This call is expected to fail because no data has arrived.
  EXPECT_EQ(-1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrPacketAccepted) {
  const WebRtc_UWord8 tmmbr_packet[] = {
    0x81, 200,  // Type 200 SR, report count = 0
    0, 6,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 1, 2, 3, 4, 5, 6, 7,  // NTP timestamp
    0, 1, 2, 3,  // RTP timestamp
    0, 0, 0, 0,  // Sender's packet count
    0, 0, 0, 0,  // Sender's octet count
    // TMMBR
    0x83, 205,  // Type 205 RTPFB, FMT 3 TMMBR
    0, 4,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 0, 1, 1,  // SSRC of media source
    2, 4, 6, 8,  // SSRC we ask to rate-control. Must match "our" SSRC.
    0, 55, 0, 0   // MxTBR
  };
  rtcp_receiver_->SetSSRC(0x2040608);  // Matches "media source" above.
  EXPECT_EQ(0, InjectRtcpPacket(tmmbr_packet, sizeof(tmmbr_packet)));
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(1);
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(1, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.ptrTmmbrSet[0]);
  EXPECT_EQ(0x101U, candidate_set.ptrSsrcSet[0]);
}

TEST_F(RtcpReceiverTest, TmmbrPacketNotForUsIgnored) {
  const WebRtc_UWord8 tmmbr_packet[] = {
    0x81, 200,  // Type 200 SR, report count = 0
    0, 6,   // length
    0, 1, 2, 3,  // SSRC of sender
    0, 1, 2, 3, 4, 5, 6, 7,  // NTP timestamp
    0, 1, 2, 3,  // RTP timestamp
    0, 0, 0, 0,  // Sender's packet count
    0, 0, 0, 0,  // Sender's octet count
    // TMMBR
    0x83, 205,  // Type 205 RTPFB, FMT 3 TMMBR
    0, 4,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 0, 1, 1,  // SSRC of media source
    99, 99, 99, 99,  // SSRC we ask to rate-control. Different from 0x2040608.
    0, 55, 0, 0   // MxTBR
  };
  rtcp_receiver_->SetSSRC(0x2040608);  // Matches "media source" above.
  EXPECT_EQ(0, InjectRtcpPacket(tmmbr_packet, sizeof(tmmbr_packet)));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrPacketZeroRateIgnored) {
  const WebRtc_UWord8 tmmbr_packet[] = {
    0x81, 200,  // Type 200 SR, report count = 0
    0, 6,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 1, 2, 3, 4, 5, 6, 7,  // NTP timestamp
    0, 1, 2, 3,  // RTP timestamp
    0, 0, 0, 0,  // Sender's packet count
    0, 0, 0, 0,  // Sender's octet count
    // TMMBR
    0x83, 205,  // Type 205 RTPFB, FMT 3 TMMBR
    0, 4,  // length
    0, 1, 2, 3,  // SSRC of sender
    0, 0, 1, 1,  // SSRC of media source
    2, 4, 6, 8,  // SSRC we ask to rate-control. Must match "our" SSRC.
    0, 0, 0, 0   // MxTBR == zero
  };
  rtcp_receiver_->SetSSRC(0x2040608);  // Matches "media source" above.
  EXPECT_EQ(0, InjectRtcpPacket(tmmbr_packet, sizeof(tmmbr_packet)));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

}  // Anonymous namespace

}  // namespace webrtc
