/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/video/payload_router.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

TEST(PayloadRouterTest, SendOnOneModule) {
  MockRtpRtcp rtp;
  std::vector<RtpRtcp*> modules(1, &rtp);

  PayloadRouter payload_router(modules);
  payload_router.SetSendingRtpModules(modules.size());

  uint8_t payload = 'a';
  FrameType frame_type = kVideoFrameKey;
  int8_t payload_type = 96;

  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1,
                                    nullptr, nullptr))
      .Times(0);
  EXPECT_FALSE(payload_router.RoutePayload(frame_type, payload_type, 0, 0,
                                           &payload, 1, nullptr, nullptr));

  payload_router.set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1,
                                    nullptr, nullptr))
      .Times(1);
  EXPECT_TRUE(payload_router.RoutePayload(frame_type, payload_type, 0, 0,
                                          &payload, 1, nullptr, nullptr));

  payload_router.set_active(false);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1,
                                    nullptr, nullptr))
      .Times(0);
  EXPECT_FALSE(payload_router.RoutePayload(frame_type, payload_type, 0, 0,
                                           &payload, 1, nullptr, nullptr));

  payload_router.set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1,
                                    nullptr, nullptr))
      .Times(1);
  EXPECT_TRUE(payload_router.RoutePayload(frame_type, payload_type, 0, 0,
                                          &payload, 1, nullptr, nullptr));

  payload_router.SetSendingRtpModules(0);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1,
                                    nullptr, nullptr))
      .Times(0);
  EXPECT_FALSE(payload_router.RoutePayload(frame_type, payload_type, 0, 0,
                                           &payload, 1, nullptr, nullptr));
}

TEST(PayloadRouterTest, SendSimulcast) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::vector<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);

  PayloadRouter payload_router(modules);
  payload_router.SetSendingRtpModules(modules.size());

  uint8_t payload_1 = 'a';
  FrameType frame_type_1 = kVideoFrameKey;
  int8_t payload_type_1 = 96;
  RTPVideoHeader rtp_hdr_1;
  rtp_hdr_1.simulcastIdx = 0;

  payload_router.set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(frame_type_1, payload_type_1, 0, 0, _, 1,
                                      nullptr, &rtp_hdr_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router.RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                          &payload_1, 1, nullptr, &rtp_hdr_1));

  uint8_t payload_2 = 'b';
  FrameType frame_type_2 = kVideoFrameDelta;
  int8_t payload_type_2 = 97;
  RTPVideoHeader rtp_hdr_2;
  rtp_hdr_2.simulcastIdx = 1;
  EXPECT_CALL(rtp_2, SendOutgoingData(frame_type_2, payload_type_2, 0, 0, _, 1,
                                      nullptr, &rtp_hdr_2))
      .Times(1);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router.RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                          &payload_2, 1, nullptr, &rtp_hdr_2));

  // Inactive.
  payload_router.set_active(false);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_FALSE(payload_router.RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                           &payload_1, 1, nullptr, &rtp_hdr_1));
  EXPECT_FALSE(payload_router.RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                           &payload_2, 1, nullptr, &rtp_hdr_2));

  // Invalid simulcast index.
  payload_router.SetSendingRtpModules(1);
  payload_router.set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  rtp_hdr_1.simulcastIdx = 1;
  EXPECT_FALSE(payload_router.RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                           &payload_1, 1, nullptr, &rtp_hdr_1));
}

TEST(PayloadRouterTest, MaxPayloadLength) {
  // Without any limitations from the modules, verify we get the max payload
  // length for IP/UDP/SRTP with a MTU of 150 bytes.
  const size_t kDefaultMaxLength = 1500 - 20 - 8 - 12 - 4;
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::vector<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  PayloadRouter payload_router(modules);

  EXPECT_EQ(kDefaultMaxLength, PayloadRouter::DefaultMaxPayloadLength());
  payload_router.SetSendingRtpModules(modules.size());

  // Modules return a higher length than the default value.
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_EQ(kDefaultMaxLength, payload_router.MaxPayloadLength());

  // The modules return a value lower than default.
  const size_t kTestMinPayloadLength = 1001;
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength));
  EXPECT_EQ(kTestMinPayloadLength, payload_router.MaxPayloadLength());
}

TEST(PayloadRouterTest, SetTargetSendBitrates) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::vector<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  PayloadRouter payload_router(modules);
  payload_router.SetSendingRtpModules(modules.size());

  const uint32_t bitrate_1 = 10000;
  const uint32_t bitrate_2 = 76543;
  std::vector<uint32_t> bitrates(2, bitrate_1);
  bitrates[1] = bitrate_2;
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(1);
  payload_router.SetTargetSendBitrates(bitrates);

  bitrates.resize(1);
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(0);
  payload_router.SetTargetSendBitrates(bitrates);
}
}  // namespace webrtc
