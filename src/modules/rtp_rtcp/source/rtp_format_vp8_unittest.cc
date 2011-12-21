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
 * This file includes unit tests for the VP8 packetizer.
 */

#include <gtest/gtest.h>

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "typedefs.h"

namespace webrtc {

template <bool>
struct CompileAssert {
};

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg) \
  typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

class RtpFormatVp8Test : public ::testing::Test {
 protected:
  RtpFormatVp8Test() : helper_(NULL) {}
  virtual void TearDown() { delete helper_; }
  bool Init() {
    const int kSizeVector[] = {10, 10, 10};
    const int kNumPartitions = sizeof(kSizeVector) / sizeof(kSizeVector[0]);
    return Init(kSizeVector, kNumPartitions);
  }
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

TEST_F(RtpFormatVp8Test, TestStrictMode) {
  ASSERT_TRUE(Init());

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kStrict);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {8, 10, 14, 5, 5, 7, 5};
  const int kExpectedPart[] = {0, 0, 1, 2, 2, 2, 2};
  const bool kExpectedFragStart[] =
      {true, false, true, true, false, false, false};
  const int kMaxSize[] = {13, 13, 20, 7, 7, 7, 7};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

TEST_F(RtpFormatVp8Test, TestAggregateMode) {
  ASSERT_TRUE(Init());

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {7, 5, 7, 23};
  const int kExpectedPart[] = {0, 0, 0, 1};
  const bool kExpectedFragStart[] = {true, false, false, true};
  const int kMaxSize[] = {8, 8, 8, 25};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

TEST_F(RtpFormatVp8Test, TestSloppyMode) {
  ASSERT_TRUE(Init());

  hdr_info_.pictureId = kNoPictureId;  // No PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kSloppy);

  // The expected sizes are obtained by running a verified good implementation.
  const int kExpectedSizes[] = {9, 9, 9, 7};
  const int kExpectedPart[] = {0, 0, 1, 2};
  const bool kExpectedFragStart[] = {true, false, false, false};
  const int kMaxSize[] = {9, 9, 9, 9};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

// Verify that sloppy mode is forced if fragmentation info is missing.
TEST_F(RtpFormatVp8Test, TestSloppyModeFallback) {
  ASSERT_TRUE(Init());

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_);

  // Expecting three full packets, and one with the remainder.
  const int kExpectedSizes[] = {10, 10, 10, 7};
  const int kExpectedPart[] = {0, 0, 0, 0};  // Always 0 for sloppy mode.
  // Frag start only true for first packet in sloppy mode.
  const bool kExpectedFragStart[] = {true, false, false, false};
  const int kMaxSize[] = {10, 10, 10, 7};  // Small enough to produce 4 packets.
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

// Verify that non-reference bit is set. Sloppy mode fallback is expected.
TEST_F(RtpFormatVp8Test, TestNonReferenceBit) {
  ASSERT_TRUE(Init());

  hdr_info_.nonReference = true;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_);

  // Sloppy mode => First packet full; other not.
  const int kExpectedSizes[] = {25, 7};
  const int kExpectedPart[] = {0, 0};  // Always 0 for sloppy mode.
  // Frag start only true for first packet in sloppy mode.
  const bool kExpectedFragStart[] = {true, false};
  const int kMaxSize[] = {25, 25};  // Small enough to produce two packets.
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->set_sloppy_partitioning(true);
  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST_F(RtpFormatVp8Test, TestTl0PicIdxAndTID) {
  ASSERT_TRUE(Init());

  hdr_info_.tl0PicIdx = 117;
  hdr_info_.temporalIdx = 2;
  hdr_info_.layerSync = true;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 4 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 4};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize[1] = {helper_->buffer_size()};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

// Verify KeyIdx field.
TEST_F(RtpFormatVp8Test, TestKeyIdx) {
  ASSERT_TRUE(Init());

  hdr_info_.keyIdx = 17;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize[1] = {helper_->buffer_size()};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

// Verify TID field and KeyIdx field in combination.
TEST_F(RtpFormatVp8Test, TestTIDAndKeyIdx) {
  ASSERT_TRUE(Init());

  hdr_info_.temporalIdx = 1;
  hdr_info_.keyIdx = 5;
  RtpFormatVp8 packetizer = RtpFormatVp8(helper_->payload_data(),
                                         helper_->payload_size(),
                                         hdr_info_,
                                         *(helper_->fragmentation()),
                                         kAggregate);

  // Expect one single packet of payload_size() + 3 bytes header.
  const int kExpectedSizes[1] = {helper_->payload_size() + 3};
  const int kExpectedPart[1] = {0};  // Packet starts with partition 0.
  const bool kExpectedFragStart[1] = {true};
  // kMaxSize is only limited by allocated buffer size.
  const int kMaxSize[1] = {helper_->buffer_size()};
  const int kExpectedNum = sizeof(kExpectedSizes) / sizeof(kExpectedSizes[0]);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedPart) / sizeof(kExpectedPart[0]),
      kExpectedPart_wrong_size);
  COMPILE_ASSERT(kExpectedNum ==
      sizeof(kExpectedFragStart) / sizeof(kExpectedFragStart[0]),
      kExpectedFragStart_wrong_size);
  COMPILE_ASSERT(kExpectedNum == sizeof(kMaxSize) / sizeof(kMaxSize[0]),
                 kMaxSize_wrong_size);

  helper_->GetAllPacketsAndCheck(&packetizer, kExpectedSizes, kExpectedPart,
                                 kExpectedFragStart, kMaxSize, kExpectedNum);
}

}  // namespace
