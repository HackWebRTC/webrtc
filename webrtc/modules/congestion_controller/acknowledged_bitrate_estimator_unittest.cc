/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/acknowledged_bitrate_estimator.h"

#include <utility>

#include "webrtc/base/fakeclock.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::InSequence;
using testing::Return;

namespace webrtc {

namespace {

constexpr int64_t first_arrival_time_ms = 10;
constexpr int64_t first_send_time_ms = 10;
constexpr uint16_t sequence_number = 1;
constexpr size_t payload_size = 10;

class MockBitrateEstimator : public BitrateEstimator {
 public:
  MOCK_METHOD2(Update, void(int64_t now_ms, int bytes));
  MOCK_CONST_METHOD0(bitrate_bps, rtc::Optional<uint32_t>());
};

class MockBitrateEstimatorCreator : public BitrateEstimatorCreator {
 public:
  MockBitrateEstimatorCreator()
      : mock_bitrate_estimator_(nullptr), num_created_bitrate_estimators_(0) {}
  std::unique_ptr<BitrateEstimator> Create() override {
    auto bitrate_estimator = rtc::MakeUnique<NiceMock<MockBitrateEstimator>>();
    mock_bitrate_estimator_ = bitrate_estimator.get();
    num_created_bitrate_estimators_++;
    return bitrate_estimator;
  }
  int num_created_bitrate_estimators() {
    return num_created_bitrate_estimators_;
  }

  MockBitrateEstimator* get_mock_bitrate_estimator() {
    RTC_CHECK(mock_bitrate_estimator_);
    return mock_bitrate_estimator_;
  }

 private:
  MockBitrateEstimator* mock_bitrate_estimator_;
  int num_created_bitrate_estimators_;
};

struct AcknowledgedBitrateEstimatorTestStates {
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator;
  MockBitrateEstimatorCreator* mock_bitrate_estimator_creator;
};

AcknowledgedBitrateEstimatorTestStates CreateTestStates() {
  AcknowledgedBitrateEstimatorTestStates states;
  auto mock_bitrate_estimator_creator =
      rtc::MakeUnique<MockBitrateEstimatorCreator>();
  states.mock_bitrate_estimator_creator = mock_bitrate_estimator_creator.get();
  states.acknowledged_bitrate_estimator =
      rtc::MakeUnique<AcknowledgedBitrateEstimator>(
          std::move(mock_bitrate_estimator_creator));
  return states;
}

std::vector<PacketFeedback> CreateFeedbackVector() {
  std::vector<PacketFeedback> packet_feedback_vector;
  const PacedPacketInfo pacing_info;
  packet_feedback_vector.push_back(
      PacketFeedback(first_arrival_time_ms, first_send_time_ms, sequence_number,
                     payload_size, pacing_info));
  packet_feedback_vector.push_back(
      PacketFeedback(first_arrival_time_ms + 10, first_send_time_ms + 10,
                     sequence_number, payload_size + 10, pacing_info));
  return packet_feedback_vector;
}

}  // anonymous namespace

TEST(TestAcknowledgedBitrateEstimator, DontAddPacketsWhichAreNotInSendHistory) {
  auto states = CreateTestStates();
  std::vector<PacketFeedback> packet_feedback_vector;
  packet_feedback_vector.push_back(
      PacketFeedback(first_arrival_time_ms, sequence_number));
  EXPECT_CALL(
      *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
      Update(_, _))
      .Times(0);
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector, false);
}

TEST(TestAcknowledgedBitrateEstimator, UpdateBandwith) {
  auto states = CreateTestStates();
  auto packet_feedback_vector = CreateFeedbackVector();
  {
    InSequence dummy;
    EXPECT_CALL(
        *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
        Update(packet_feedback_vector[0].arrival_time_ms,
               static_cast<int>(packet_feedback_vector[0].payload_size)))
        .Times(1);
    EXPECT_CALL(
        *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
        Update(packet_feedback_vector[1].arrival_time_ms,
               static_cast<int>(packet_feedback_vector[1].payload_size)))
        .Times(1);
  }
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector, false);
}

TEST(TestAcknowledgedBitrateEstimator,
     ResetAfterLeafAlrStateAndDontAddOldPackets) {
  auto states = CreateTestStates();
  auto packet_feedback_vector = CreateFeedbackVector();
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector, true);

  rtc::ScopedFakeClock fake_clock;

  fake_clock.AdvanceTime(
      rtc::TimeDelta::FromMilliseconds(first_arrival_time_ms + 1));

  EXPECT_EQ(
      1,
      states.mock_bitrate_estimator_creator->num_created_bitrate_estimators());
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      std::vector<PacketFeedback>(), false);
  EXPECT_EQ(
      2,
      states.mock_bitrate_estimator_creator->num_created_bitrate_estimators());

  {
    InSequence dummy;
    EXPECT_CALL(
        *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
        Update(packet_feedback_vector[0].arrival_time_ms,
               static_cast<int>(packet_feedback_vector[0].payload_size)))
        .Times(0);
    EXPECT_CALL(
        *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
        Update(packet_feedback_vector[1].arrival_time_ms,
               static_cast<int>(packet_feedback_vector[1].payload_size)))
        .Times(1);
  }
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector, false);
}

TEST(TestAcknowledgedBitrateEstimator, ReturnBitrate) {
  auto states = CreateTestStates();
  rtc::Optional<uint32_t> return_value(42);
  EXPECT_CALL(
      *states.mock_bitrate_estimator_creator->get_mock_bitrate_estimator(),
      bitrate_bps())
      .Times(1)
      .WillOnce(Return(return_value));
  EXPECT_EQ(return_value, states.acknowledged_bitrate_estimator->bitrate_bps());
}

}  // namespace webrtc
