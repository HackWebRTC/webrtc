/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// This file includes unit tests for EncoderStateFeedback.
#include "webrtc/video/encoder_state_feedback.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/video/vie_encoder.h"

using ::testing::NiceMock;

namespace webrtc {

class MockVieEncoder : public ViEEncoder {
 public:
  explicit MockVieEncoder(ProcessThread* process_thread, PacedSender* pacer)
      : ViEEncoder(1,
                   std::vector<uint32_t>(),
                   process_thread,
                   nullptr,
                   nullptr,
                   nullptr,
                   pacer,
                   nullptr) {}
  ~MockVieEncoder() {}

  MOCK_METHOD1(OnReceivedIntraFrameRequest,
               void(uint32_t));
  MOCK_METHOD2(OnReceivedSLI,
               void(uint32_t ssrc, uint8_t picture_id));
  MOCK_METHOD2(OnReceivedRPSI,
               void(uint32_t ssrc, uint64_t picture_id));
};

TEST(VieKeyRequestTest, CreateAndTriggerRequests) {
  static const uint32_t kSsrc = 1234;
  NiceMock<MockProcessThread> process_thread;
  PacketRouter router;
  PacedSender pacer(Clock::GetRealTimeClock(), &router,
                     BitrateController::kDefaultStartBitrateKbps,
                     PacedSender::kDefaultPaceMultiplier *
                         BitrateController::kDefaultStartBitrateKbps,
                     0);
  MockVieEncoder encoder(&process_thread, &pacer);

  EncoderStateFeedback encoder_state_feedback;
  encoder_state_feedback.Init(std::vector<uint32_t>(1, kSsrc), &encoder);

  EXPECT_CALL(encoder, OnReceivedIntraFrameRequest(kSsrc))
      .Times(1);
  encoder_state_feedback.OnReceivedIntraFrameRequest(kSsrc);

  const uint8_t sli_picture_id = 3;
  EXPECT_CALL(encoder, OnReceivedSLI(kSsrc, sli_picture_id))
      .Times(1);
  encoder_state_feedback.OnReceivedSLI(kSsrc, sli_picture_id);

  const uint64_t rpsi_picture_id = 9;
  EXPECT_CALL(encoder, OnReceivedRPSI(kSsrc, rpsi_picture_id))
      .Times(1);
  encoder_state_feedback.OnReceivedRPSI(kSsrc, rpsi_picture_id);
}

}  // namespace webrtc
