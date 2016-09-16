/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/stats/rtcstats.h"

#include <cstring>

#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"

namespace webrtc {

class RTCTestStats : public RTCStats {
 public:
  RTCTestStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        m_int32("mInt32"),
        m_uint32("mUint32"),
        m_int64("mInt64"),
        m_uint64("mUint64"),
        m_double("mDouble"),
        m_string("mString"),
        m_sequence_int32("mSequenceInt32"),
        m_sequence_uint32("mSequenceUint32"),
        m_sequence_int64("mSequenceInt64"),
        m_sequence_uint64("mSequenceUint64"),
        m_sequence_double("mSequenceDouble"),
        m_sequence_string("mSequenceString") {
  }

  WEBRTC_RTCSTATS_IMPL(RTCStats, RTCTestStats,
      &m_int32,
      &m_uint32,
      &m_int64,
      &m_uint64,
      &m_double,
      &m_string,
      &m_sequence_int32,
      &m_sequence_uint32,
      &m_sequence_int64,
      &m_sequence_uint64,
      &m_sequence_double,
      &m_sequence_string);

  RTCStatsMember<int32_t> m_int32;
  RTCStatsMember<uint32_t> m_uint32;
  RTCStatsMember<int64_t> m_int64;
  RTCStatsMember<uint64_t> m_uint64;
  RTCStatsMember<double> m_double;
  RTCStatsMember<std::string> m_string;

  RTCStatsMember<std::vector<int32_t>> m_sequence_int32;
  RTCStatsMember<std::vector<uint32_t>> m_sequence_uint32;
  RTCStatsMember<std::vector<int64_t>> m_sequence_int64;
  RTCStatsMember<std::vector<uint64_t>> m_sequence_uint64;
  RTCStatsMember<std::vector<double>> m_sequence_double;
  RTCStatsMember<std::vector<std::string>> m_sequence_string;
};

const char RTCTestStats::kType[] = "test-stats";

class RTCChildStats : public RTCStats {
 public:
  RTCChildStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        child_int("childInt") {}

  WEBRTC_RTCSTATS_IMPL(RTCStats, RTCChildStats,
      &child_int);

  RTCStatsMember<int32_t> child_int;
};

const char RTCChildStats::kType[] = "child-stats";

class RTCGrandChildStats : public RTCChildStats {
 public:
  RTCGrandChildStats(const std::string& id, int64_t timestamp_us)
      : RTCChildStats(id, timestamp_us),
        grandchild_int("grandchildInt") {}

  WEBRTC_RTCSTATS_IMPL(RTCChildStats, RTCGrandChildStats,
      &grandchild_int);

  RTCStatsMember<int32_t> grandchild_int;
};

const char RTCGrandChildStats::kType[] = "grandchild-stats";

TEST(RTCStatsTest, RTCStatsAndMembers) {
  RTCTestStats stats("testId", 42);
  EXPECT_EQ(stats.id(), "testId");
  EXPECT_EQ(stats.timestamp_us(), static_cast<int64_t>(42));
  std::vector<const RTCStatsMemberInterface*> members = stats.Members();
  EXPECT_EQ(members.size(), static_cast<size_t>(12));
  for (const RTCStatsMemberInterface* member : members) {
    EXPECT_FALSE(member->is_defined());
  }
  stats.m_int32 = 123;
  stats.m_uint32 = 123;
  stats.m_int64 = 123;
  stats.m_uint64 = 123;
  stats.m_double = 123.0;
  stats.m_string = std::string("123");

  std::vector<int32_t> sequence_int32;
  sequence_int32.push_back(static_cast<int32_t>(1));
  std::vector<uint32_t> sequence_uint32;
  sequence_uint32.push_back(static_cast<uint32_t>(2));
  std::vector<int64_t> sequence_int64;
  sequence_int64.push_back(static_cast<int64_t>(3));
  std::vector<uint64_t> sequence_uint64;
  sequence_uint64.push_back(static_cast<uint64_t>(4));
  std::vector<double> sequence_double;
  sequence_double.push_back(5.0);
  std::vector<std::string> sequence_string;
  sequence_string.push_back(std::string("six"));

  stats.m_sequence_int32 = sequence_int32;
  stats.m_sequence_uint32 = sequence_uint32;
  EXPECT_FALSE(stats.m_sequence_int64.is_defined());
  stats.m_sequence_int64 = sequence_int64;
  stats.m_sequence_uint64 = sequence_uint64;
  stats.m_sequence_double = sequence_double;
  stats.m_sequence_string = sequence_string;
  for (const RTCStatsMemberInterface* member : members) {
    EXPECT_TRUE(member->is_defined());
  }
  EXPECT_EQ(*stats.m_int32, static_cast<int32_t>(123));
  EXPECT_EQ(*stats.m_uint32, static_cast<uint32_t>(123));
  EXPECT_EQ(*stats.m_int64, static_cast<int64_t>(123));
  EXPECT_EQ(*stats.m_uint64, static_cast<uint64_t>(123));
  EXPECT_EQ(*stats.m_double, 123.0);
  EXPECT_EQ(*stats.m_string, std::string("123"));
  EXPECT_EQ(*stats.m_sequence_int32, sequence_int32);
  EXPECT_EQ(*stats.m_sequence_uint32, sequence_uint32);
  EXPECT_EQ(*stats.m_sequence_int64, sequence_int64);
  EXPECT_EQ(*stats.m_sequence_uint64, sequence_uint64);
  EXPECT_EQ(*stats.m_sequence_double, sequence_double);
  EXPECT_EQ(*stats.m_sequence_string, sequence_string);

  int32_t numbers[] = { 4, 8, 15, 16, 23, 42 };
  std::vector<int32_t> numbers_sequence(&numbers[0], &numbers[6]);
  stats.m_sequence_int32->clear();
  stats.m_sequence_int32->insert(stats.m_sequence_int32->end(),
                                 numbers_sequence.begin(),
                                 numbers_sequence.end());
  EXPECT_EQ(*stats.m_sequence_int32, numbers_sequence);
}

TEST(RTCStatsTest, RTCStatsGrandChild) {
  RTCGrandChildStats stats("grandchild", 0.0);
  stats.child_int = 1;
  stats.grandchild_int = 2;
  int32_t sum = 0;
  for (const RTCStatsMemberInterface* member : stats.Members()) {
    sum += *member->cast_to<const RTCStatsMember<int32_t>>();
  }
  EXPECT_EQ(sum, static_cast<int32_t>(3));

  std::unique_ptr<RTCStats> copy_ptr = stats.copy();
  const RTCGrandChildStats& copy = copy_ptr->cast_to<RTCGrandChildStats>();
  EXPECT_EQ(*copy.child_int, *stats.child_int);
  EXPECT_EQ(*copy.grandchild_int, *stats.grandchild_int);
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(RTCStatsDeathTest, ValueOfUndefinedMember) {
  RTCTestStats stats("testId", 0.0);
  EXPECT_FALSE(stats.m_int32.is_defined());
  EXPECT_DEATH(*stats.m_int32, "");
}

TEST(RTCStatsDeathTest, InvalidCasting) {
  RTCGrandChildStats stats("grandchild", 0.0);
  EXPECT_DEATH(stats.cast_to<RTCChildStats>(), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
