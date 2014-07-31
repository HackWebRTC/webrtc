/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for the VP8 packetizer.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/typedefs.h"

#define CHECK_ARRAY_SIZE(expected_size, array)                       \
  COMPILE_ASSERT(expected_size == sizeof(array) / sizeof(array[0]),  \
                 check_array_size);

namespace webrtc {

class RtpPacketizerVp8Test : public ::testing::Test {
 protected:
  RtpPacketizerVp8Test() : helper_(NULL) {}
  virtual void TearDown() { delete helper_; }
  bool Init(const int* partition_sizes, int num_partitions) {
    hdr_info_.pictureId = kNoPictureId;
    hdr_info_.nonReference = false;
    hdr_info_.temporalIdx = kNoTemporalIdx;
    hdr_info_.layerSync = false;
    hdr_info_.tl0PicIdx = kNoTl0PicIdx;
    hdr_info_.keyIdx = kNoKeyIdx;
    if (helper_ != NULL) return false;
    helper_ = new test::RtpFormatVp8TestHelper(&hdr_info_);
    return helper_->Init(partition_sizes, num_partitions);
  }

  RTPVideoHeaderVP8 hdr_info_;
  test::RtpFormatVp8TestHelper* helper_;
};

TEST_F(RtpPacketizerVp8Test, TestStrictMode) {
  const int kSizeVector[] = {10, 8, 27};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  const int kMaxSize = 13;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kStrict);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {9, 9, 12, 11, 11, 11, 10};
  const int kExpectedPart[] = {0, 0, 1, 2, 2, 2, 2};
  const bool kExpectedFragStart[] =
      {true, false, true, true, false, false, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateMode) {
  const int kSizeVector[] = {60, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const int kMaxSize = 25;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {23, 23, 23, 23};
  const int kExpectedPart[] = {0, 0, 0, 1};
  const bool kExpectedFragStart[] = {true, false, false, true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeManyPartitions1) {
  const int kSizeVector[] = {1600, 200, 200, 200, 200, 200, 200, 200, 200};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const int kMaxSize = 1500;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {803, 803, 803, 803};
  const int kExpectedPart[] = {0, 0, 1, 5};
  const bool kExpectedFragStart[] = {true, false, true, true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeManyPartitions2) {
  const int kSizeVector[] = {1599, 200, 200, 200, 1600, 200, 200, 200, 200};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const int kMaxSize = 1500;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {803, 802, 603, 803, 803, 803};
  const int kExpectedPart[] = {0, 0, 1, 4, 4, 5};
  const bool kExpectedFragStart[] = {true, false, true, true, false, true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

TEST_F(RtpPacketizerVp8Test, TestAggregateModeTwoLargePartitions) {
  const int kSizeVector[] = {1654, 2268};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  const int kMaxSize = 1460;
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {830, 830, 1137, 1137};
  const int kExpectedPart[] = {0, 0, 1, 1};
  const bool kExpectedFragStart[] = {true, false, true, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify that EqualSize mode is forced if fragmentation info is missing.
TEST_F(RtpPacketizerVp8Test, TestEqualSizeModeFallback) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID
  const int kMaxSize = 12;  // Small enough to produce 4 packets.
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize);
  packetizer.SetPayloadData(
      helper_->payload_data(), helper_->payload_size(), NULL);

  // Expecting three full packets, and one with the remainder.
  const int kExpectedSizes[] = {12, 11, 12, 11};
  const int kExpectedPart[] = {0, 0, 0, 0};  // Always 0 for equal size mode.
  // Frag start only true for first packet in equal size mode.
  const bool kExpectedFragStart[] = {true, false, false, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify that non-reference bit is set. EqualSize mode fallback is expected.
TEST_F(RtpPacketizerVp8Test, TestNonReferenceBit) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.nonReference = true;
  const int kMaxSize = 25;  // Small enough to produce two packets.
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize);
  packetizer.SetPayloadData(
      helper_->payload_data(), helper_->payload_size(), NULL);

  // EqualSize mode => First packet full; other not.
  const int kExpectedSizes[] = {16, 16};
  const int kExpectedPart[] = {0, 0};  // Always 0 for equal size mode.
  // Frag start only true for first packet in equal size mode.
  const bool kExpectedFragStart[] = {true, false};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST_F(RtpPacketizerVp8Test, TestTl0PicIdxAndTID) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.tl0PicIdx = 117;
  hdr_info_.temporalIdx = 2;
  hdr_info_.layerSync = true;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // Expect one single packet of payload_size() + 4 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 4};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify KeyIdx field.
TEST_F(RtpPacketizerVp8Test, TestKeyIdx) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.keyIdx = 17;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

// Verify TID field and KeyIdx field in combination.
TEST_F(RtpPacketizerVp8Test, TestTIDAndKeyIdx) {
  const int kSizeVector[] = {10, 10, 10};
  const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
  ASSERT_TRUE(Init(kSizeVector, kNumPartitions));

  hdr_info_.temporalIdx = 1;
  hdr_info_.keyIdx = 5;
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize = helper_->buffer_size();
  RtpPacketizerVp8 packetizer(hdr_info_, kMaxSize, kAggregate);
  packetizer.SetPayloadData(helper_->payload_data(),
                            helper_->payload_size(),
                            helper_->fragmentation());

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedPart);
  CHECK_ARRAY_SIZE(kExpectedNum, kExpectedFragStart);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kExpectedNum);
}

}  // namespace
