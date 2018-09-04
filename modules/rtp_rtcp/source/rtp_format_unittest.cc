/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format.h"

#include <memory>
#include <numeric>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Le;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Calculate difference between largest and smallest packets respecting sizes
// adjustement provided by limits,
// i.e. last packet expected to be smaller than 'average' by reduction_len.
int EffectivePacketsSizeDifference(
    std::vector<size_t> sizes,
    const RtpPacketizer::PayloadSizeLimits& limits) {
  // Account for larger last packet header.
  sizes.back() += limits.last_packet_reduction_len;

  auto minmax = std::minmax_element(sizes.begin(), sizes.end());
  // MAX-MIN
  return *minmax.second - *minmax.first;
}

size_t Sum(const std::vector<size_t>& sizes) {
  return std::accumulate(sizes.begin(), sizes.end(), 0);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualSumToPayloadLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_THAT(Sum(payload_sizes), 13);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualRespectsMaxPayloadSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerSplitAboutEqually,
     AllPacketsAreEqualRespectsLastPacketReductionLength) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(13, limits);

  ASSERT_THAT(payload_sizes, Not(IsEmpty()));
  EXPECT_LE(payload_sizes.back() + limits.last_packet_reduction_len,
            limits.max_payload_len);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualInSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_EQ(EffectivePacketsSizeDifference(payload_sizes, limits), 0);
}

TEST(RtpPacketizerSplitAboutEqually,
     AllPacketsAreEqualGeneratesMinimumNumberOfPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(13, limits);
  // Computed by hand. 3 packets would have exactly capacity 3*5-2=13
  // (max length - for each packet minus last packet reduction).
  EXPECT_THAT(payload_sizes, SizeIs(3));
}

TEST(RtpPacketizerSplitAboutEqually, SomePacketsAreSmallerSumToPayloadLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_THAT(Sum(payload_sizes), 28);
}

TEST(RtpPacketizerVideoGeneric, SomePacketsAreSmallerRespectsMaxPayloadSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerVideoGeneric,
     SomePacketsAreSmallerRespectsLastPacketReductionLength) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_LE(payload_sizes.back(),
            limits.max_payload_len - limits.last_packet_reduction_len);
}

TEST(RtpPacketizerVideoGeneric, SomePacketsAreSmallerPacketsAlmostEqualInSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_LE(EffectivePacketsSizeDifference(payload_sizes, limits), 1);
}

TEST(RtpPacketizerVideoGeneric,
     SomePacketsAreSmallerGeneratesMinimumNumberOfPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<size_t> payload_sizes =
      RtpPacketizer::SplitAboutEqually(24, limits);
  // Computed by hand. 4 packets would have capacity 4*7-5=23 (max length -
  // for each packet minus last packet reduction).
  // 5 packets is enough for kPayloadSize.
  EXPECT_THAT(payload_sizes, SizeIs(5));
}

}  // namespace
}  // namespace webrtc
