/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for the TransmissionBucket.
 */

#include <gtest/gtest.h>

#include "rtp_rtcp_defines.h"
#include "transmission_bucket.h"
#include "typedefs.h"

namespace webrtc {

// TODO(asapersson): This class has been introduced in several test files.
// Break out into a unittest helper file.
class FakeClock : public RtpRtcpClock {
 public:
  FakeClock() {
    time_in_ms_ = 123456;
  }
  // Return a timestamp in milliseconds relative to some arbitrary
  // source; the source is fixed for this clock.
  virtual WebRtc_Word64 GetTimeInMS() {
    return time_in_ms_;
  }
  // Retrieve an NTP absolute timestamp.
  virtual void CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac) {
    secs = time_in_ms_ / 1000;
    frac = (time_in_ms_ % 1000) * 4294967;
  }
  void IncrementTime(WebRtc_UWord32 time_increment_ms) {
    time_in_ms_ += time_increment_ms;
  }
 private:
  int64_t time_in_ms_;
};

class TransmissionBucketTest : public ::testing::Test {
 protected:
  TransmissionBucketTest()
      : send_bucket_(new TransmissionBucket(&fake_clock_)) {
  }
  ~TransmissionBucketTest() {
    delete send_bucket_;
  }
  FakeClock fake_clock_;
  TransmissionBucket* send_bucket_;
};

TEST_F(TransmissionBucketTest, Fill) {
  EXPECT_TRUE(send_bucket_->Empty());
  send_bucket_->Fill(1, 3000, 100);
  EXPECT_FALSE(send_bucket_->Empty());
}

TEST_F(TransmissionBucketTest, Reset) {
  send_bucket_->Fill(1, 3000, 100);
  EXPECT_FALSE(send_bucket_->Empty());
  send_bucket_->Reset();
  EXPECT_TRUE(send_bucket_->Empty());
}

TEST_F(TransmissionBucketTest, GetNextPacket) {
  EXPECT_EQ(-1, send_bucket_->GetNextPacket());    // empty

  const int delta_time_ms = 1;
  const int target_bitrate_kbps = 800;  // 150 bytes per interval
  send_bucket_->UpdateBytesPerInterval(delta_time_ms, target_bitrate_kbps);

  send_bucket_->Fill(1235, 3000, 75);
  send_bucket_->Fill(1236, 3000, 75);

  EXPECT_EQ(1235, send_bucket_->GetNextPacket());  // ok
  EXPECT_EQ(1236, send_bucket_->GetNextPacket());  // ok
  EXPECT_TRUE(send_bucket_->Empty());

  send_bucket_->Fill(1237, 3000, 75);
  EXPECT_EQ(-1, send_bucket_->GetNextPacket());    // packet does not fit
}

TEST_F(TransmissionBucketTest, SameFrameAndPacketIntervalTimeElapsed) {
  const int delta_time_ms = 1;
  const int target_bitrate_kbps = 800;  // 150 bytes per interval
  send_bucket_->UpdateBytesPerInterval(delta_time_ms, target_bitrate_kbps);

  send_bucket_->Fill(1235, 3000, 75);
  send_bucket_->Fill(1236, 3000, 75);

  EXPECT_EQ(1235, send_bucket_->GetNextPacket());  // ok
  EXPECT_EQ(1236, send_bucket_->GetNextPacket());  // ok
  EXPECT_TRUE(send_bucket_->Empty());

  fake_clock_.IncrementTime(4);
  send_bucket_->Fill(1237, 3000, 75);
  EXPECT_EQ(-1, send_bucket_->GetNextPacket());    // packet does not fit

  // Time limit (5ms) elapsed.
  fake_clock_.IncrementTime(1);
  EXPECT_EQ(1237, send_bucket_->GetNextPacket());
}

TEST_F(TransmissionBucketTest, NewFrameAndFrameIntervalTimeElapsed) {
  const int delta_time_ms = 1;
  const int target_bitrate_kbps = 800;  // 150 bytes per interval
  send_bucket_->UpdateBytesPerInterval(delta_time_ms, target_bitrate_kbps);

  send_bucket_->Fill(1235, 3000, 75);
  send_bucket_->Fill(1236, 3000, 75);

  EXPECT_EQ(1235, send_bucket_->GetNextPacket());  // ok
  EXPECT_EQ(1236, send_bucket_->GetNextPacket());  // ok
  EXPECT_TRUE(send_bucket_->Empty());

  fake_clock_.IncrementTime(4);
  send_bucket_->Fill(1237, 6000, 75);
  EXPECT_EQ(-1, send_bucket_->GetNextPacket());    // packet does not fit

  // Time limit elapsed (4*1.2=4.8ms).
  fake_clock_.IncrementTime(1);
  EXPECT_EQ(1237, send_bucket_->GetNextPacket());
}
}  // namespace webrtc
