/*
 * libjingle
 * Copyright 2010, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/gunit.h"
#include "talk/base/ratetracker.h"

namespace talk_base {

class RateTrackerForTest : public RateTracker {
 public:
  RateTrackerForTest() : time_(0) {}
  virtual uint32 Time() const { return time_; }
  void AdvanceTime(uint32 delta) { time_ += delta; }

 private:
  uint32 time_;
};

TEST(RateTrackerTest, TestBasics) {
  RateTrackerForTest tracker;
  EXPECT_EQ(0U, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Add a sample.
  tracker.Update(1234);
  // Advance the clock by 100 ms.
  tracker.AdvanceTime(100);
  // total_units should advance, but units_second should stay 0.
  EXPECT_EQ(1234U, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Repeat.
  tracker.Update(1234);
  tracker.AdvanceTime(100);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Advance the clock by 800 ms, so we've elapsed a full second.
  // units_second should now be filled in properly.
  tracker.AdvanceTime(800);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(1234U * 2, tracker.units_second());

  // Poll the tracker again immediately. The reported rate should stay the same.
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(1234U * 2, tracker.units_second());

  // Do nothing and advance by a second. We should drop down to zero.
  tracker.AdvanceTime(1000);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Send a bunch of data at a constant rate for 5.5 "seconds".
  // We should report the rate properly.
  for (int i = 0; i < 5500; i += 100) {
    tracker.Update(9876U);
    tracker.AdvanceTime(100);
  }
  EXPECT_EQ(9876U * 10, tracker.units_second());

  // Advance the clock by 500 ms. Since we sent nothing over this half-second,
  // the reported rate should be reduced by half.
  tracker.AdvanceTime(500);
  EXPECT_EQ(9876U * 5, tracker.units_second());
}

}  // namespace talk_base
