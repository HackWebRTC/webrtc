/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <type_traits>

#include "quiche.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

TEST(QuicheAbiTest, ExposesStableEarlyDataReasons) {
  EXPECT_EQ(QUICHE_EARLY_DATA_UNKNOWN, 0);
  EXPECT_EQ(QUICHE_EARLY_DATA_ACCEPTED, 2);
  EXPECT_EQ(QUICHE_EARLY_DATA_QUIC_PARAMETER_MISMATCH, 13);
  EXPECT_EQ(QUICHE_EARLY_DATA_ALPS_MISMATCH, 14);
}

TEST(QuicheAbiTest, ExposesPacingDelayAndDatagramLossFields) {
  quiche_send_info send_info = {};
  send_info.pacing_delay_nanos = 7;
  EXPECT_EQ(send_info.pacing_delay_nanos, 7u);

  quiche_path_stats path_stats = {};
  path_stats.dgram_lost = 9;
  EXPECT_EQ(path_stats.dgram_lost, 9u);
  static_assert(std::is_same_v<decltype(path_stats.max_rtt), uint64_t>);
}

}  // namespace
}  // namespace webrtc::roq
