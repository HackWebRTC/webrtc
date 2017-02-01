/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/mod_ops.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

using webrtc::rtcp::TransportFeedback;
using testing::Return;
using testing::StrictMock;

namespace webrtc {

namespace {

constexpr size_t kMaxConsecutiveOldReports = 4;

// All tests are run multiple times with various baseline sequence number,
// to weed out potential bugs with wrap-around handling.
constexpr uint16_t kBases[] = {0x0000, 0x3456, 0xc032, 0xfffe};

void AddTransportFeedbackAndValidate(
    TransportFeedbackPacketLossTracker* tracker,
    uint16_t base_sequence_num,
    const std::vector<bool>& reception_status_vec) {
  const int64_t kBaseTimeUs = 1234;  // Irrelevant to this test.
  TransportFeedback test_feedback;
  test_feedback.SetBase(base_sequence_num, kBaseTimeUs);
  uint16_t sequence_num = base_sequence_num;
  for (bool status : reception_status_vec) {
    if (status)
      test_feedback.AddReceivedPacket(sequence_num, kBaseTimeUs);
    ++sequence_num;
  }

  // TransportFeedback imposes some limitations on what constitutes a legal
  // status vector. For instance, the vector cannot terminate in a lost
  // packet. Make sure all limitations are abided by.
  RTC_CHECK_EQ(base_sequence_num, test_feedback.GetBaseSequence());
  const auto& vec = test_feedback.GetStatusVector();
  RTC_CHECK_EQ(reception_status_vec.size(), vec.size());
  for (size_t i = 0; i < reception_status_vec.size(); i++) {
    RTC_CHECK_EQ(reception_status_vec[i],
                 vec[i] != TransportFeedback::StatusSymbol::kNotReceived);
  }

  tracker->OnReceivedTransportFeedback(test_feedback);
  tracker->Validate();
}

// Checks that validty is as expected. If valid, checks also that
// value is as expected.
void ValidatePacketLossStatistics(
    const TransportFeedbackPacketLossTracker& tracker,
    rtc::Optional<float> expected_plr,
    rtc::Optional<float> expected_rplr) {
  // Comparing the rtc::Optional<float> directly would have given concise code,
  // but less readable error messages.
  rtc::Optional<float> plr = tracker.GetPacketLossRate();
  EXPECT_EQ(static_cast<bool>(expected_plr), static_cast<bool>(plr));
  if (expected_plr && plr) {
    EXPECT_EQ(*expected_plr, *plr);
  }

  rtc::Optional<float> rplr = tracker.GetRecoverablePacketLossRate();
  EXPECT_EQ(static_cast<bool>(expected_rplr), static_cast<bool>(rplr));
  if (expected_rplr && rplr) {
    EXPECT_EQ(*expected_rplr, *rplr);
  }
}

// Convenience function for when both are valid, and explicitly stating
// the rtc::Optional<float> constructor is just cumbersome.
void ValidatePacketLossStatistics(
    const TransportFeedbackPacketLossTracker& tracker,
    float expected_plr,
    float expected_rplr) {
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(expected_plr),
                               rtc::Optional<float>(expected_rplr));
}

}  // namespace

// Sanity check on an empty window.
TEST(TransportFeedbackPacketLossTrackerTest, EmptyWindow) {
  std::unique_ptr<TransportFeedback> feedback;
  TransportFeedbackPacketLossTracker tracker(10, 5, 5);

  // PLR and RPLR reported as unknown before reception of first feedback.
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());
}

// Sanity check on partially filled window.
TEST(TransportFeedbackPacketLossTrackerTest, PlrPartiallyFilledWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 4);

    // PLR unknown before minimum window size reached.
    // RPLR unknown before minimum pairs reached.
    // Expected window contents: [] -> [1001].
    AddTransportFeedbackAndValidate(&tracker, base, {true, false, false, true});
    ValidatePacketLossStatistics(tracker,
                                 rtc::Optional<float>(),
                                 rtc::Optional<float>());
  }
}

// Sanity check on minimum filled window - PLR known, RPLR unknown.
TEST(TransportFeedbackPacketLossTrackerTest, PlrMinimumFilledWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 5);

    // PLR correctly calculated after minimum window size reached.
    // RPLR not necessarily known at that time (not if min-pairs not reached).
    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker,
                                 rtc::Optional<float>(2.0f / 5.0f),
                                 rtc::Optional<float>());
  }
}

// Sanity check on minimum filled window - PLR unknown, RPLR known.
TEST(TransportFeedbackPacketLossTrackerTest, RplrMinimumFilledWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 6, 4);

    // RPLR correctly calculated after minimum pairs reached.
    // PLR not necessarily known at that time (not if min window not reached).
    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker,
                                 rtc::Optional<float>(),
                                 rtc::Optional<float>(1.0f / 4.0f));
  }
}

// Additional reports update PLR and RPLR.
TEST(TransportFeedbackPacketLossTrackerTest, ExtendWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 5);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker,
                                 rtc::Optional<float>(2.0f / 5.0f),
                                 rtc::Optional<float>());

    // Expected window contents: [10011] -> [1001110101].
    AddTransportFeedbackAndValidate(&tracker, base + 5,
                                    {true, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

    // Expected window contents: [1001110101] -> [1001110101-GAP-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 20,
                                    {true, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 7.0f / 15.0f, 4.0f / 13.0f);
  }
}

// All packets correctly received.
TEST(TransportFeedbackPacketLossTrackerTest, AllReceived) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 4);

    // PLR and RPLR correctly calculated after minimum window size reached.
    // Expected window contents: [] -> [11111].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, true, true, true, true});
    ValidatePacketLossStatistics(tracker, 0.0f, 0.0f);
  }
}

// Repeated reports are ignored.
TEST(TransportFeedbackPacketLossTrackerTest, ReportRepetition) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 4);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

    // Repeat entire previous feedback
    // Expected window contents: [10011] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
  }
}

// Report overlap.
TEST(TransportFeedbackPacketLossTrackerTest, ReportOverlap) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

    // Expected window contents: [10011] -> [1001101].
    AddTransportFeedbackAndValidate(&tracker, base + 3,
                                    {true, true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 6.0f);
  }
}

// Report conflict.
TEST(TransportFeedbackPacketLossTrackerTest, ReportConflict) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 4);

    // Expected window contents: [] -> [01001].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {false, true, false, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 5.0f, 2.0f / 4.0f);

    // Expected window contents: [01001] -> [11101].
    // While false->true will be applied, true -> false will be ignored.
    AddTransportFeedbackAndValidate(&tracker, base, {true, false, true});
    ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);
  }
}

// Skipped packets treated as unknown (not lost).
TEST(TransportFeedbackPacketLossTrackerTest, SkippedPackets) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

    // Expected window contents: [10011] -> [10011-GAP-101].
    AddTransportFeedbackAndValidate(&tracker, base + 100, {true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 6.0f);
  }
}

// The window retain information up to the configured max-window-size, but
// starts discarding after that.
TEST(TransportFeedbackPacketLossTrackerTest, MaxWindowSize) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 10, 1);

    // Up to max-window-size retained.
    // Expected window contents: [] -> [1010100001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, true, false, true, false, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 6.0f / 10.0f, 3.0f / 9.0f);

    // After max-window-size, older entries discarded to accommodate newer ones.
    // Expected window contents: [1010100001] -> [0000110111].
    AddTransportFeedbackAndValidate(&tracker, base + 10,
                                    {true, false, true, true, true});
    ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 2.0f / 9.0f);
  }
}

// Inserting into the middle of a full window works correctly.
TEST(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddle) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(10, 5, 1);

    // Expected window contents: [] -> [10101].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

    // Expected window contents: [10101] -> [10101-GAP-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 100,
                                    {true, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 3.0f / 8.0f);

    // Insert into the middle of this full window - it discards the older data.
    // Expected window contents: [10101-GAP-10001] -> [11111-GAP-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 50,
                                    {true, true, true, true, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 1.0f / 8.0f);
  }
}

// Inserting into the middle of a full window works correctly.
TEST(TransportFeedbackPacketLossTrackerTest, InsertionCompletesTwoPairs) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(15, 5, 1);

    // Expected window contents: [] -> [10111].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, true, true, true});
    ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);

    // Expected window contents: [10111] -> [10111-GAP-10101].
    AddTransportFeedbackAndValidate(&tracker, base + 7,
                                    {true, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 3.0f / 8.0f);

    // Insert in between, closing the gap completely.
    // Expected window contents: [10111-GAP-10101] -> [101111010101].
    AddTransportFeedbackAndValidate(&tracker, base + 5, {false, true});
    ValidatePacketLossStatistics(tracker, 4.0f / 12.0f, 4.0f / 11.0f);
  }
}

// Entries in the second quadrant treated like those in the first.
// The sequence number is used in a looped manner. 0xFFFF is followed by 0x0000.
// In many tests, we divide the circle of sequence number into 4 quadrants, and
// verify the behavior of TransportFeedbackPacketLossTracker over them.
TEST(TransportFeedbackPacketLossTrackerTest, SecondQuadrant) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

    // Window *does* get updated with inputs from quadrant #2.
    // Expected window contents: [10011] -> [100111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4321, {true});
    ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 1.0f / 4.0f);

    // Correct recognition of quadrant #2: up to, but not including, base +
    // 0x8000
    // Expected window contents: [100111] -> [1001111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x7fff, {true});
    ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 1.0f / 4.0f);
  }
}

// Insertion into the third quadrant moves the base of the window.
TEST(TransportFeedbackPacketLossTrackerTest, ThirdQuadrantMovesBase) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Seed the test.
    // Expected window contents: [] -> [1001101].
    AddTransportFeedbackAndValidate(
        &tracker, base, {true, false, false, true, true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 6.0f);

    // Quadrant #3 begins at base + 0x8000. It triggers moving the window so
    // that
    // at least one (oldest) report shifts out of window.
    // Expected window contents: [1001101] -> [101-GAP-1001].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 5.0f);

    // The base can move more than once, because the minimum quadrant-1 packets
    // were dropped out of the window, and some remain.
    // Expected window contents: [101-GAP-1001] -> [1-GAP-100111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000 + 4, {true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 1.0f / 5.0f);
  }
}

// After the base has moved due to insertion into the third quadrant, it is
// still possible to insert into the middle of the window and obtain the correct
// PLR and RPLR. Insertion into the middle before the max window size has been
// achieved does not cause older packets to be dropped.
TEST(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddleAfterBaseMove) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Seed the test.
    // Expected window contents: [] -> [1001101].
    AddTransportFeedbackAndValidate(
        &tracker, base, {true, false, false, true, true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 6.0f);

    // Expected window contents: [1001101] -> [101-GAP-1001].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 5.0f);

    // Inserting into the middle still works after the base has shifted.
    // Expected window contents:
    // [101-GAP-1001] -> [101-GAP-100101-GAP-1001]
    AddTransportFeedbackAndValidate(&tracker, base + 0x5000,
                                    {true, false, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 6.0f / 13.0f, 4.0f / 10.0f);

    // The base can keep moving after inserting into the middle.
    // Expected window contents:
    // [101-GAP-100101-GAP-1001] -> [1-GAP-100101-GAP-100111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000 + 4, {true, true});
    ValidatePacketLossStatistics(tracker, 5.0f / 13.0f, 3.0f / 10.0f);
  }
}

// After moving the base of the window, the max window size is still observed.
TEST(TransportFeedbackPacketLossTrackerTest, ThirdQuadrantObservesMaxWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(15, 10, 1);

    // Expected window contents: [] -> [1001110101].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

    // Expected window contents: [1001110101] -> [1110101-GAP-101].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 3.0f / 8.0f);

    // Push into middle until max window is reached.
    // Expected window contents:
    // [1110101-GAP-101] -> [1110101-GAP-10001-GAP-101]
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000,
                                    {true, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 6.0f / 15.0f, 4.0f / 12.0f);

    // Pushing new packets into the middle would discard older packets.
    // Expected window contents:
    // [1110101-GAP-10001-GAP-101] -> [0101-GAP-10001101-GAP-101]
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000 + 5,
                                    {true, false, true});
    ValidatePacketLossStatistics(tracker, 7.0f / 15.0f, 5.0f / 12.0f);
  }
}

// A new feedback in quadrant #3 might shift enough old feedbacks out of window,
// that we'd go back to an unknown PLR and RPLR.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantThreeMovedBaseMinWindow) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [1001110101].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, true, false, true});
    ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

    // A new feedback in quadrant #3 might shift enough old feedbacks out of
    // window, that we'd go back to an unknown PLR and RPLR. This *doesn't*
    // necessarily mean all of the old ones were discarded, though.
    // Expected window contents: [1001110101] -> [01-GAP-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8006, {true, true});
    ValidatePacketLossStatistics(tracker,
                                 rtc::Optional<float>(),  // Still invalid.
                                 rtc::Optional<float>(1.0f / 2.0f));

    // Inserting in the middle shows that though some of the elements were
    // ejected, some were retained.
    // Expected window contents: [01-GAP-11] -> [01-GAP-1001-GAP-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000,
                                    {true, false, false, true});
    ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 5.0f);
  }
}

// Quadrant four reports ignored for up to kMaxConsecutiveOldReports times.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourInitiallyIgnored) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
    }
  }
}

// Receiving a packet from quadrant #1 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ1) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
    }

    // If we receive a feedback in quadrant #1, the above counter is reset.
    // Expected window contents: [10011] -> [100111].
    AddTransportFeedbackAndValidate(&tracker, base + 5, {true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, false, true});
      ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 1.0f / 5.0f);
    }

    // The same is true for reports which create a gap - they still reset.
    // Expected window contents: [10011] -> [100111-GAP-01].
    AddTransportFeedbackAndValidate(&tracker, base + 0x00ff, {false, true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, false, true});
      ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 6.0f);
    }
  }
}

// Receiving a packet from quadrant #2 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ2) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
    }

    // If we receive a feedback in quadrant #2, the above counter is reset.
    // Expected window contents: [10011] -> [10011-GAP-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x400f, {true, true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, false, true});
      ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 1.0f / 5.0f);
    }
  }
}

// Receiving a packet from quadrant #3 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ3) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [1001110001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, false, false, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [1001110001] -> [1001110001].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 2.0f / 9.0f);
    }

    // If we receive a feedback in quadrant #1, the above counter is reset.
    // Expected window contents: [1001110001] -> [1110001-GAP-111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, true, true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000 + 10,
                                      {true, false, true});
      ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 1.0f / 8.0f);
    }
  }
}

// Quadrant four reports ignored for up to kMaxConsecutiveOldReports times.
// After that, the window is reset.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourReset) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [1001110001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, false, false, true});

    // Sanity
    ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 2.0f / 9.0f);

    // The first kMaxConsecutiveOldReports quadrant #4 reports are ignored.
    // It doesn't matter that they consist of multiple packets - each report
    // is only counted once.
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Expected window contents: [1001110001] -> [1001110001].
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, true, false, true});
      ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 2.0f / 9.0f);
    }

    // One additional feedback in quadrant #4 brings us over
    // kMaxConsecutiveOldReports consecutive "old" reports, resetting the
    // window.
    // The new window is not completely empty - it's been seeded with the
    // packets reported in the feedback that has triggered the reset.
    // Note: The report doesn't have to be the same as the previous ones.
    // Expected window contents: [1001110001] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q1-Q2).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ1Q2) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

    // A feedback with entries in both quadrant #1 and #2 gets both counted:
    // Expected window contents: [10011] -> [10011-GAP-1001].
    AddTransportFeedbackAndValidate(&tracker, base + 0x3ffe,
                                    {true, false, false, true});
    ValidatePacketLossStatistics(tracker, 4.0f / 9.0f, 2.0f / 7.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q2-Q3).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ2Q3) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [1001100001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, false, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 6.0f / 10.0f, 2.0f / 9.0f);

    // A feedback with entries in both quadrant #2 and #3 gets both counted,
    // but only those from #3 trigger throwing out old entries from quadrant #1:
    // Expected window contents: [1001100001] -> [01100001-GAP-1001].
    AddTransportFeedbackAndValidate(&tracker, base + 0x7ffe,
                                    {true, false, false, true});
    ValidatePacketLossStatistics(tracker, 7.0f / 12.0f, 3.0f / 10.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q3-Q4).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ3Q4) {
  for (uint16_t base : kBases) {
    TransportFeedbackPacketLossTracker tracker(20, 5, 1);

    // Expected window contents: [] -> [1001100001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, false, false, false, false, true});
    ValidatePacketLossStatistics(tracker, 6.0f / 10.0f, 2.0f / 9.0f);

    // A feedback with entries in both quadrant #3 and #4 would have the entries
    // from quadrant #3 shift enough quadrant #1 entries out of window, that
    // by the time the #4 packets are examined, the moving baseline has made
    // them into quadrant #3 packets.
    // Expected window contents: [1001100001] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base + 0xbfff,
                                    {true, false, false, true, true});
    ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
  }
}

}  // namespace webrtc
