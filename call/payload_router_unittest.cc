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
#include <string>

#include "call/payload_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Unused;

namespace webrtc {
namespace {
const int8_t kPayloadType = 96;
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialPictureId2 = 44;
const int16_t kInitialTl0PicIdx1 = 99;
const int16_t kInitialTl0PicIdx2 = 199;
}  // namespace

TEST(PayloadRouterTest, SendOnOneModule) {
  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules(1, &rtp);

  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image._timeStamp = 1;
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;

  PayloadRouter payload_router(modules, {kSsrc1}, kPayloadType, {});

  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(0);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(true);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp, Sending()).WillOnce(Return(true));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(false);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(0);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(true);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp, Sending()).WillOnce(Return(true));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);
}

TEST(PayloadRouterTest, SendSimulcastSetActive) {
  NiceMock<MockRtpRtcp> rtp_1;
  NiceMock<MockRtpRtcp> rtp_2;
  std::vector<RtpRtcp*> modules = {&rtp_1, &rtp_2};

  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image._timeStamp = 1;
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;

  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});

  CodecSpecificInfo codec_info_1;
  memset(&codec_info_1, 0, sizeof(CodecSpecificInfo));
  codec_info_1.codecType = kVideoCodecVP8;
  codec_info_1.codecSpecific.VP8.simulcastIdx = 0;

  payload_router.SetActive(true);
  EXPECT_CALL(rtp_1, Sending()).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                      encoded_image._timeStamp,
                                      encoded_image.capture_time_ms_, &payload,
                                      encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);

  CodecSpecificInfo codec_info_2;
  memset(&codec_info_2, 0, sizeof(CodecSpecificInfo));
  codec_info_2.codecType = kVideoCodecVP8;
  codec_info_2.codecSpecific.VP8.simulcastIdx = 1;

  EXPECT_CALL(rtp_2, Sending()).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                      encoded_image._timeStamp,
                                      encoded_image.capture_time_ms_, &payload,
                                      encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_2, nullptr)
                .error);

  // Inactive.
  payload_router.SetActive(false);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_2, nullptr)
                .error);
}

// Tests how setting individual rtp modules to active affects the overall
// behavior of the payload router. First sets one module to active and checks
// that outgoing data can be sent on this module, and checks that no data can be
// sent if both modules are inactive.
TEST(PayloadRouterTest, SendSimulcastSetActiveModules) {
  NiceMock<MockRtpRtcp> rtp_1;
  NiceMock<MockRtpRtcp> rtp_2;
  std::vector<RtpRtcp*> modules = {&rtp_1, &rtp_2};

  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image._timeStamp = 1;
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  CodecSpecificInfo codec_info_1;
  memset(&codec_info_1, 0, sizeof(CodecSpecificInfo));
  codec_info_1.codecType = kVideoCodecVP8;
  codec_info_1.codecSpecific.VP8.simulcastIdx = 0;
  CodecSpecificInfo codec_info_2;
  memset(&codec_info_2, 0, sizeof(CodecSpecificInfo));
  codec_info_2.codecType = kVideoCodecVP8;
  codec_info_2.codecSpecific.VP8.simulcastIdx = 1;

  // Only setting one stream to active will still set the payload router to
  // active and allow sending data on the active stream.
  std::vector<bool> active_modules({true, false});
  payload_router.SetActiveModules(active_modules);

  EXPECT_CALL(rtp_1, Sending()).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                      encoded_image._timeStamp,
                                      encoded_image.capture_time_ms_, &payload,
                                      encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);

  // Setting both streams to inactive will turn the payload router to inactive.
  active_modules = {false, false};
  payload_router.SetActiveModules(active_modules);
  // An incoming encoded image will not ask the module to send outgoing data
  // because the payload router is inactive.
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_1, Sending()).Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, Sending()).Times(0);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_2, nullptr)
                .error);
}

TEST(PayloadRouterTest, CreateWithNoPreviousStates) {
  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  payload_router.SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      payload_router.GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_NE(initial_states.find(kSsrc1), initial_states.end());
  EXPECT_NE(initial_states.find(kSsrc2), initial_states.end());
}

TEST(PayloadRouterTest, CreateWithPreviousStates) {
  RtpPayloadState state1;
  state1.picture_id = kInitialPictureId1;
  state1.tl0_pic_idx = kInitialTl0PicIdx1;
  RtpPayloadState state2;
  state2.picture_id = kInitialPictureId2;
  state2.tl0_pic_idx = kInitialTl0PicIdx2;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc1, state1},
                                                {kSsrc2, state2}};

  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, states);
  payload_router.SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      payload_router.GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_EQ(kInitialPictureId1, initial_states[kSsrc1].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, initial_states[kSsrc1].tl0_pic_idx);
  EXPECT_EQ(kInitialPictureId2, initial_states[kSsrc2].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx2, initial_states[kSsrc2].tl0_pic_idx);
}
}  // namespace webrtc
