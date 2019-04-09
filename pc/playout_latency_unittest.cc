/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "pc/playout_latency.h"
#include "pc/test/mock_delayable.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Return;

namespace {
constexpr int kSsrc = 1234;
}  // namespace

namespace webrtc {

class PlayoutLatencyTest : public ::testing::Test {
 public:
  PlayoutLatencyTest()
      : latency_(
            new rtc::RefCountedObject<PlayoutLatency>(rtc::Thread::Current())) {
  }

 protected:
  rtc::scoped_refptr<PlayoutLatencyInterface> latency_;
  MockDelayable delayable_;
};

TEST_F(PlayoutLatencyTest, DefaultValue) {
  EXPECT_DOUBLE_EQ(0.0, latency_->GetLatency());
}

TEST_F(PlayoutLatencyTest, GetLatency) {
  latency_->OnStart(&delayable_, kSsrc);

  EXPECT_CALL(delayable_, GetBaseMinimumPlayoutDelayMs(kSsrc))
      .WillOnce(Return(2000));
  // Latency in seconds.
  EXPECT_DOUBLE_EQ(2.0, latency_->GetLatency());

  EXPECT_CALL(delayable_, GetBaseMinimumPlayoutDelayMs(kSsrc))
      .WillOnce(Return(absl::nullopt));
  // When no value is returned by GetBaseMinimumPlayoutDelayMs, and there are
  // no caching, then return default value.
  EXPECT_DOUBLE_EQ(0.0, latency_->GetLatency());
}

TEST_F(PlayoutLatencyTest, SetLatency) {
  latency_->OnStart(&delayable_, kSsrc);

  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 3000))
      .WillOnce(Return(true));

  // Latency in seconds.
  latency_->SetLatency(3.0);
}

TEST_F(PlayoutLatencyTest, Caching) {
  // Check that value is cached before start.
  latency_->SetLatency(4.0);
  // Latency in seconds.
  EXPECT_DOUBLE_EQ(4.0, latency_->GetLatency());

  // Check that cached value applied on the start.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 4000))
      .WillOnce(Return(true));
  latency_->OnStart(&delayable_, kSsrc);

  EXPECT_CALL(delayable_, GetBaseMinimumPlayoutDelayMs(kSsrc))
      .WillOnce(Return(absl::nullopt));
  // On false the latest cached value is returned.
  EXPECT_DOUBLE_EQ(4.0, latency_->GetLatency());

  latency_->OnStop();

  // Check that after stop it returns last cached value.
  EXPECT_DOUBLE_EQ(4.0, latency_->GetLatency());
}

TEST_F(PlayoutLatencyTest, Clamping) {
  latency_->OnStart(&delayable_, kSsrc);

  // In current Jitter Buffer implementation (Audio or Video) maximum supported
  // value is 10000 milliseconds.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 10000))
      .WillOnce(Return(true));
  latency_->SetLatency(10.5);

  // Boundary value in seconds to milliseconds conversion.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 0))
      .WillOnce(Return(true));
  latency_->SetLatency(0.0009);

  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 0))
      .WillOnce(Return(true));

  latency_->SetLatency(-2.0);
}

}  // namespace webrtc
