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
  tracker->OnReceivedTransportFeedback(test_feedback);
  tracker->Validate();
}

}  // namespace

// Sanity check on an empty window.
TEST(TransportFeedbackPacketLossTrackerTest, EmptyWindow) {
  std::unique_ptr<TransportFeedback> feedback;
  float plr = 0.0f;   // Packet-loss-rate
  float cplr = 0.0f;  // Consecutive-packet-loss-rate

  TransportFeedbackPacketLossTracker tracker(5, 10);

  // PLR and CPLR reported as unknown before reception of first feedback.
  EXPECT_FALSE(tracker.GetPacketLossRates(&plr, &cplr));
}

// Sanity check on partially filled window.
TEST(TransportFeedbackPacketLossTrackerTest, PartiallyFilledWindow) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // PLR and CPLR reported as unknown before minimum window size reached.
    // Expected window contents: [] -> [1001].
    AddTransportFeedbackAndValidate(&tracker, base, {true, false, false, true});
    EXPECT_FALSE(tracker.GetPacketLossRates(&plr, &cplr));
  }
}

// Sanity check on minimum filled window.
TEST(TransportFeedbackPacketLossTrackerTest, MinimumFilledWindow) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // PLR and CPLR correctly calculated after minimum window size reached.
    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 5.0f);
    EXPECT_EQ(cplr, 1.0f / 5.0f);
  }
}

// Additional reports update PLR and CPLR.
TEST(TransportFeedbackPacketLossTrackerTest, ExtendWindow) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Expected window contents: [10011] -> [10011-10101].
    AddTransportFeedbackAndValidate(&tracker, base + 5,
                                    {true, false, true, false, true});

    // Expected window contents: [10011-10101] -> [10011-10101-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 10,
                                    {true, false, false, false, true});

    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 7.0f / 15.0f);
    EXPECT_EQ(cplr, 3.0f / 15.0f);
  }
}

TEST(TransportFeedbackPacketLossTrackerTest, AllReceived) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // PLR and CPLR correctly calculated after minimum window size reached.
    // Expected window contents: [] -> [11111].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, true, true, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 0.0f);
    EXPECT_EQ(cplr, 0.0f);
  }
}

// Repeated reports are ignored.
TEST(TransportFeedbackPacketLossTrackerTest, ReportRepetition) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Repeat entire previous feedback
    // Expected window contents: [10011] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 5.0f);
    EXPECT_EQ(cplr, 1.0f / 5.0f);
  }
}

// Report overlap.
TEST(TransportFeedbackPacketLossTrackerTest, ReportOverlap) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Expected window contents: [10011] -> [10011-01].
    AddTransportFeedbackAndValidate(&tracker, base + 3,
                                    {true, true, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 3.0f / 7.0f);
    EXPECT_EQ(cplr, 1.0f / 7.0f);
  }
}

// Report conflict.
TEST(TransportFeedbackPacketLossTrackerTest, ReportConflict) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // Expected window contents: [] -> [01001].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {false, true, false, false, true});

    // Expected window contents: [01001] -> [11101].
    // While false->true will be applied, true -> false will be ignored.
    AddTransportFeedbackAndValidate(&tracker, base, {true, false, true});

    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 1.0f / 5.0f);
    EXPECT_EQ(cplr, 0.0f / 5.0f);
  }
}

// Skipped packets treated as unknown (not lost).
TEST(TransportFeedbackPacketLossTrackerTest, SkippedPackets) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 10);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Expected window contents: [10011] -> [10011-101].
    AddTransportFeedbackAndValidate(&tracker, base + 100, {true, false, true});

    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 3.0f / 8.0f);
    EXPECT_EQ(cplr, 1.0f / 8.0f);
  }
}

// The window retain information up to the configured max-window-size, but
// starts discarding after that.
TEST(TransportFeedbackPacketLossTrackerTest, MaxWindowSize) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(10, 10);

    // Expected window contents: [] -> [10101-00001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, true, false, true, false, false, false, false, true});

    // Up to max-window-size retained.
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 6.0f / 10.0f);
    EXPECT_EQ(cplr, 3.0f / 10.0f);

    // Expected window contents: [10101-00001] -> [00001-10111].
    AddTransportFeedbackAndValidate(&tracker, base + 10,
                                    {true, false, true, true, true});

    // After max-window-size, older entries discarded to accommodate newer ones.
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 5.0f / 10.0f);
    EXPECT_EQ(cplr, 3.0f / 10.0f);
  }
}

// Inserting into the middle of a full window works correctly.
TEST(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddle) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(10, 10);

    // Expected window contents: [] -> [10101].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, true, false, true});

    // Expected window contents: [10101] -> [10101-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 100,
                                    {true, false, false, false, true});

    // Setup sanity
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 5.0f / 10.0f);
    EXPECT_EQ(cplr, 2.0f / 10.0f);

    // Insert into the middle of this full window - it discards the older data.
    // Expected window contents: [10101-10001] -> [11111-10001].
    AddTransportFeedbackAndValidate(&tracker, base + 50,
                                    {true, true, true, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 3.0f / 10.0f);
    EXPECT_EQ(cplr, 2.0f / 10.0f);
  }
}

// Test the behavior of TransportFeedbackPacketLossTracker when there is a gap
// of more than 0x4000 in sequence number, i.e., 1/4 of total sequence numbers.
// Since the sequence number is used in a circular manner, i.e., after 0xffff,
// the sequence number wraps back to 0x0000, we refer to 1/4 of total sequence
// numbers as a quadrant. In this test, e.g., three transport feedbacks are
// added, whereas the 2nd and 3rd lie in the second quadrant w.r.t. the 1st
// feedback.
TEST(TransportFeedbackPacketLossTrackerTest, SecondQuadrant) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Window *does* get updated with inputs from quadrant #2.
    // Expected window contents: [10011] -> [10011-1].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4321, {true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 6.0f);
    EXPECT_EQ(cplr, 1.0f / 6.0f);

    // Correct recognition of quadrant #2: up to, but not including, base +
    // 0x8000
    // Expected window contents: [10011-1] -> [10011-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x7fff, {true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 7.0f);
    EXPECT_EQ(cplr, 1.0f / 7.0f);
  }
}

// Insertion into the third quadrant moves the base of the window.
TEST(TransportFeedbackPacketLossTrackerTest, ThirdQuadrantMovesBase) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Seed the test.
    // Expected window contents: [] -> [10011-01].
    AddTransportFeedbackAndValidate(
        &tracker, base, {true, false, false, true, true, false, true});

    // Quadrant #3 begins at base + 0x8000. It triggers moving the window so
    // that
    // at least one (oldest) report shifts out of window.
    // Expected window contents: [10011-01] -> [10110-01].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 3.0f / 7.0f);
    EXPECT_EQ(cplr, 1.0f / 7.0f);

    // The base can move more than once, because the minimum quadrant-1 packets
    // were dropped out of the window, and some remain.
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000 + 4, {true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 7.0f);
    EXPECT_EQ(cplr, 1.0f / 7.0f);
  }
}

// After the base has moved due to insertion into the third quadrant, it is
// still possible to insert into the middle of the window and obtain the correct
// PLR and CPLR. Insertion into the middle before the max window size has been
// achieved does not cause older packets to be dropped.
TEST(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddleAfterBaseMove) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Seed the test.
    // Expected window contents: [] -> [10011-01].
    AddTransportFeedbackAndValidate(
        &tracker, base, {true, false, false, true, true, false, true});

    // Expected window contents: [10011-01] -> [10110-01].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, false, true});

    // Inserting into the middle still works after the base has shifted.
    // Expected window contents: [10110-01] -> [10110-01011-001].
    AddTransportFeedbackAndValidate(&tracker, base + 0x5000,
                                    {true, false, false, true, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 6.0f / 13.0f);
    EXPECT_EQ(cplr, 2.0f / 13.0f);

    // The base can keep moving after inserting into the middle.
    // Expected window contents: [10110-01011-001] -> [11001-01100-111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000 + 4, {true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 5.0f / 13.0f);
    EXPECT_EQ(cplr, 2.0f / 13.0f);
  }
}

// After moving the base of the window, the max window size is still observed.
TEST(TransportFeedbackPacketLossTrackerTest, ThirdQuadrantObservesMaxWindow) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(10, 15);

    // Expected window contents: [] -> [10011-10101].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, true, false, true});

    // Expected window contents: [10011-10101] -> [11101-01101].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, false, true});

    // Push into middle until max window is reached.
    // Expected window contents: [11101-01101] -> [11101-01100-01101].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000,
                                    {true, false, false, false, true});

    // Setup sanity
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 6.0f / 15.0f);
    EXPECT_EQ(cplr, 2.0f / 15.0f);

    // Pushing new packets into the middle would discard older packets.
    // Expected window contents: [11101-01100-01101] -> [01011-00011-01101].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000 + 5,
                                    {true, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 7.0f / 15.0f);
    EXPECT_EQ(cplr, 2.0f / 15.0f);
  }
}

// A new feedback in quadrant #3 might shift enough old feedbacks out of window,
// that we'd go back to an unknown PLR and CPLR.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantThreeMovedBaseMinWindow) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011-10101].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, true, false, true});
    EXPECT_TRUE(
        tracker.GetPacketLossRates(&plr, &cplr));  // Min window reached.

    // A new feedback in quadrant #3 might shift enough old feedbacks out of
    // window, that we'd go back to an unknown PLR and CPLR. This *doesn't*
    // necessarily mean all of the old ones were discarded, though.
    // Expected window contents: [10011-10101] -> [0111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8006, {true, true});
    EXPECT_FALSE(tracker.GetPacketLossRates(&plr, &cplr));

    // Inserting in the middle shows that though some of the elements were
    // ejected, some were retained.
    // Expected window contents: [] -> [01101-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x4000,
                                    {true, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 7.0f);
    EXPECT_EQ(cplr, 0.0f / 7.0f);
  }
}

// Quadrant four reports ignored for up to kMaxConsecutiveOldReports times.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourInitiallyIgnored) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 2.0f / 5.0f);
      EXPECT_EQ(cplr, 1.0f / 5.0f);
    }
  }
}

// Receiving a packet from quadrant #1 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ1) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 2.0f / 5.0f);
      EXPECT_EQ(cplr, 1.0f / 5.0f);
    }

    // If we receive a feedback in quadrant #1, the above counter is reset.
    // Expected window contents: [10011] -> [10011-1].
    AddTransportFeedbackAndValidate(&tracker, base + 0x000f, {true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, false, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 2.0f / 6.0f);
      EXPECT_EQ(cplr, 1.0f / 6.0f);
    }
  }
}

// Receiving a packet from quadrant #2 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ2) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011] -> [10011].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 2.0f / 5.0f);
      EXPECT_EQ(cplr, 1.0f / 5.0f);
    }

    // If we receive a feedback in quadrant #1, the above counter is reset.
    // Expected window contents: [10011] -> [10011-1].
    AddTransportFeedbackAndValidate(&tracker, base + 0x400f, {true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, false, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 2.0f / 6.0f);
      EXPECT_EQ(cplr, 1.0f / 6.0f);
    }
  }
}

// Receiving a packet from quadrant #3 resets the counter for quadrant #4.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourCounterResetByQ3) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011-10001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, false, false, true});

    // Feedbacks in quadrant #4 are discarded (up to kMaxConsecutiveOldReports
    // consecutive reports).
    // Expected window contents: [10011-10001] -> [10011-10001].
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000, {true, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 5.0f / 10.0f);
      EXPECT_EQ(cplr, 3.0f / 10.0f);
    }

    // If we receive a feedback in quadrant #1, the above counter is reset.
    // Expected window contents: [10011-10001] -> [11100-01111].
    AddTransportFeedbackAndValidate(&tracker, base + 0x8000,
                                    {true, true, true});
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Note: though the feedback message reports three packets, it only gets
      // counted once.
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000 + 10,
                                      {true, false, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 3.0f / 10.0f);
      EXPECT_EQ(cplr, 2.0f / 10.0f);
    }
  }
}

// Quadrant four reports ignored for up to kMaxConsecutiveOldReports times.
// After that, the window is reset.
TEST(TransportFeedbackPacketLossTrackerTest, QuadrantFourReset) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011-10001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, true, false, false, false, true});

    // The first kMaxConsecutiveOldReports quadrant #4 reports are ignored.
    // It doesn't matter that they consist of multiple packets - each report
    // is only counted once.
    for (size_t i = 0; i < kMaxConsecutiveOldReports; i++) {
      // Expected window contents: [10011-10001] -> [10011-10001].
      AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                      {true, true, false, true});
      EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
      EXPECT_EQ(plr, 5.0f / 10.0f);
      EXPECT_EQ(cplr, 3.0f / 10.0f);
    }

    // One additional feedback in quadrant #4 brings us over
    // kMaxConsecutiveOldReports consecutive "old" reports, resetting the
    // window.
    // Note: The report doesn't have to be the same as the previous ones.
    // Expected window contents: [10011-10001] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base + 0xc000,
                                    {true, false, false, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));

    // The new window is not completely empty - it's been seeded with the
    // packets reported in the feedback that has triggered the reset.
    EXPECT_EQ(plr, 2.0f / 5.0f);
    EXPECT_EQ(cplr, 1.0f / 5.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q1-Q2).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ1Q2) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base,
                                    {true, false, false, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));

    // A feedback with entries in both quadrant #1 and #2 gets both counted:
    // Expected window contents: [10011] -> [10011-11].
    AddTransportFeedbackAndValidate(&tracker, base + 0x3fff, {true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 7.0f);
    EXPECT_EQ(cplr, 1.0f / 7.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q2-Q3).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ2Q3) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate
    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011-00001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, false, false, false, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 6.0f / 10.0f);
    EXPECT_EQ(cplr, 4.0f / 10.0f);

    // A feedback with entries in both quadrant #2 and #3 gets both counted,
    // but only those from #3 trigger throwing out old entries from quadrant #1:
    // Expected window contents: [10011-00001] -> [01100-00110-01].
    AddTransportFeedbackAndValidate(&tracker, base + 0x7ffe,
                                    {true, false, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 7.0f / 12.0f);
    EXPECT_EQ(cplr, 4.0f / 12.0f);
  }
}

// Feedbacks spanning multiple quadrant are treated correctly (Q2-Q3).
TEST(TransportFeedbackPacketLossTrackerTest, MultiQuadrantQ3Q4) {
  for (uint16_t base : kBases) {
    float plr = 0.0f;   // Packet-loss-rate
    float cplr = 0.0f;  // Consecutive-packet-loss-rate

    TransportFeedbackPacketLossTracker tracker(5, 20);

    // Expected window contents: [] -> [10011-00001].
    AddTransportFeedbackAndValidate(
        &tracker, base,
        {true, false, false, true, true, false, false, false, false, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 6.0f / 10.0f);
    EXPECT_EQ(cplr, 4.0f / 10.0f);

    // A feedback with entries in both quadrant #3 and #4 would have the entries
    // from quadrant #3 shift enough quadrant #1 entries out of window, that
    // by the time the #4 packets are examined, the moving baseline has made
    // them into quadrant #3 packets.
    // Expected window contents: [10011-00001] -> [10011].
    AddTransportFeedbackAndValidate(&tracker, base + 0xbfff,
                                    {true, false, false, true, true});
    EXPECT_TRUE(tracker.GetPacketLossRates(&plr, &cplr));
    EXPECT_EQ(plr, 2.0f / 5.0f);
    EXPECT_EQ(cplr, 1.0f / 5.0f);
  }
}

}  // namespace webrtc
