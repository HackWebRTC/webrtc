/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <list>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/video_engine/payload_router.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class PayloadRouterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    payload_router_.reset(new PayloadRouter());
  }
  rtc::scoped_ptr<PayloadRouter> payload_router_;
};

TEST_F(PayloadRouterTest, SendOnOneModule) {
  MockRtpRtcp rtp;
  std::list<RtpRtcp*> modules(1, &rtp);

  payload_router_->SetSendingRtpModules(modules);

  uint8_t payload = 'a';
  FrameType frame_type = kVideoFrameKey;
  int8_t payload_type = 96;

  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));

  payload_router_->set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(1);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                            &payload, 1, NULL, NULL));

  payload_router_->set_active(false);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));

  payload_router_->set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(1);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                            &payload, 1, NULL, NULL));

  modules.clear();
  payload_router_->SetSendingRtpModules(modules);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));
}

TEST_F(PayloadRouterTest, SendSimulcast) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);

  payload_router_->SetSendingRtpModules(modules);

  uint8_t payload_1 = 'a';
  FrameType frame_type_1 = kVideoFrameKey;
  int8_t payload_type_1 = 96;
  RTPVideoHeader rtp_hdr_1;
  rtp_hdr_1.simulcastIdx = 0;

  payload_router_->set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(frame_type_1, payload_type_1, 0, 0, _, 1,
                                      NULL, &rtp_hdr_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                            &payload_1, 1, NULL, &rtp_hdr_1));

  uint8_t payload_2 = 'b';
  FrameType frame_type_2 = kVideoFrameDelta;
  int8_t payload_type_2 = 97;
  RTPVideoHeader rtp_hdr_2;
  rtp_hdr_2.simulcastIdx = 1;
  EXPECT_CALL(rtp_2, SendOutgoingData(frame_type_2, payload_type_2, 0, 0, _, 1,
                                      NULL, &rtp_hdr_2))
      .Times(1);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                            &payload_2, 1, NULL, &rtp_hdr_2));

  // Inactive.
  payload_router_->set_active(false);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                             &payload_1, 1, NULL, &rtp_hdr_1));
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                             &payload_2, 1, NULL, &rtp_hdr_2));

  // Invalid simulcast index.
  payload_router_->set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  rtp_hdr_1.simulcastIdx = 2;
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                             &payload_1, 1, NULL, &rtp_hdr_1));
}

TEST_F(PayloadRouterTest, MaxPayloadLength) {
  // Without any limitations from the modules, verify we get the max payload
  // length for IP/UDP/SRTP with a MTU of 150 bytes.
  const size_t kDefaultMaxLength = 1500 - 20 - 8 - 12 - 4;
  EXPECT_EQ(kDefaultMaxLength, payload_router_->DefaultMaxPayloadLength());
  EXPECT_EQ(kDefaultMaxLength, payload_router_->MaxPayloadLength());

  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  // Modules return a higher length than the default value.
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_EQ(kDefaultMaxLength, payload_router_->MaxPayloadLength());

  // The modules return a value lower than default.
  const size_t kTestMinPayloadLength = 1001;
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength));
  EXPECT_EQ(kTestMinPayloadLength, payload_router_->MaxPayloadLength());
}

TEST_F(PayloadRouterTest, TimeToSendPacket) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  const uint16_t kSsrc1 = 1234;
  uint16_t sequence_number = 17;
  uint64_t timestamp = 7890;
  bool retransmission = false;

  // Send on the first module by letting rtp_1 be sending with correct ssrc.
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC())
      .Times(1)
      .WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_1, TimeToSendPacket(kSsrc1, sequence_number, timestamp,
                                      retransmission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->TimeToSendPacket(
      kSsrc1, sequence_number, timestamp, retransmission));


  // Send on the second module by letting rtp_2 be sending, but not rtp_1.
  ++sequence_number;
  timestamp += 30;
  retransmission = true;
  const uint16_t kSsrc2 = 4567;
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_2, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC())
      .Times(1)
      .WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(kSsrc2, sequence_number, timestamp,
                                      retransmission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(payload_router_->TimeToSendPacket(
      kSsrc2, sequence_number, timestamp, retransmission));

  // No module is sending, hence no packet should be sent.
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _,_))
      .Times(0);
  EXPECT_CALL(rtp_2, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->TimeToSendPacket(
      kSsrc1, sequence_number, timestamp, retransmission));

  // Add a packet with incorrect ssrc and test it's dropped in the router.
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC())
      .Times(1)
      .WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_2, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC())
      .Times(1)
      .WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _,_))
      .Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->TimeToSendPacket(
      kSsrc1 + kSsrc2, sequence_number, timestamp, retransmission));
}

TEST_F(PayloadRouterTest, TimeToSendPadding) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  // Default configuration, sending padding on the first sending module.
  const size_t requested_padding_bytes = 1000;
  const size_t sent_padding_bytes = 890;
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_))
      .Times(0);
  EXPECT_EQ(sent_padding_bytes,
            payload_router_->TimeToSendPadding(requested_padding_bytes));

  // Let only the second module be sending and verify the padding request is
  // routed there.
  EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes))
      .Times(0);
  EXPECT_CALL(rtp_2, SendingMedia())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_EQ(sent_padding_bytes,
            payload_router_->TimeToSendPadding(requested_padding_bytes));

  // No sending module at all.
   EXPECT_CALL(rtp_1, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes))
      .Times(0);
  EXPECT_CALL(rtp_2, SendingMedia())
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_))
      .Times(0);
  EXPECT_EQ(static_cast<size_t>(0),
            payload_router_->TimeToSendPadding(requested_padding_bytes));
}

TEST_F(PayloadRouterTest, SetTargetSendBitrates) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  const uint32_t bitrate_1 = 10000;
  const uint32_t bitrate_2 = 76543;
  std::vector<uint32_t> bitrates (2, bitrate_1);
  bitrates[1] = bitrate_2;
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(1);
  payload_router_->SetTargetSendBitrates(bitrates);

  bitrates.resize(1);
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(0);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(0);
  payload_router_->SetTargetSendBitrates(bitrates);

  bitrates.resize(3);
  bitrates[1] = bitrate_2;
  bitrates[2] = bitrate_1 + bitrate_2;
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(1);
  payload_router_->SetTargetSendBitrates(bitrates);
  }
}  // namespace webrtc
