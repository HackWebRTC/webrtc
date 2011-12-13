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
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

const int kPayloadSize = 30;
const int kBufferSize = kPayloadSize + 6;  // Add space for payload descriptor.

class RtpFormatVp8Test : public ::testing::Test {
 protected:
  RtpFormatVp8Test() {}
  virtual void SetUp();
  virtual void TearDown();
  void CheckHeader(bool first_in_frame, bool frag_start, int part_id);
  void CheckPictureID();
  void CheckTl0PicIdx();
  void CheckTIDAndKeyIdx();
  void CheckPayload(int payload_end);
  void CheckLast(bool last) const;
  void CheckPacket(int send_bytes, int expect_bytes, bool last,
                   bool first_in_frame, bool frag_start);
  void CheckPacketZeroPartId(int send_bytes, int expect_bytes, bool last,
                             bool first_in_frame, bool frag_start);
  WebRtc_UWord8 payload_data_[kPayloadSize];
  WebRtc_UWord8 buffer_[kBufferSize];
  WebRtc_UWord8 *data_ptr_;
  RTPFragmentationHeader* fragmentation_;
  RTPVideoHeaderVP8 hdr_info_;
  int payload_start_;
};

void RtpFormatVp8Test::SetUp() {
  for (int i = 0; i < kPayloadSize; i++) {
    payload_data_[i] = i / 10;  // Integer division.
  }
  data_ptr_ = payload_data_;

  fragmentation_ = new RTPFragmentationHeader;
  fragmentation_->VerifyAndAllocateFragmentationHeader(3);
  fragmentation_->fragmentationLength[0] = 10;
  fragmentation_->fragmentationLength[1] = 10;
  fragmentation_->fragmentationLength[2] = 10;
  fragmentation_->fragmentationOffset[0] = 0;
  fragmentation_->fragmentationOffset[1] = 10;
  fragmentation_->fragmentationOffset[2] = 20;

  hdr_info_.pictureId = kNoPictureId;
  hdr_info_.nonReference = false;
  hdr_info_.temporalIdx = kNoTemporalIdx;
  hdr_info_.tl0PicIdx = kNoTl0PicIdx;
  hdr_info_.keyIdx = kNoKeyIdx;
}

void RtpFormatVp8Test::TearDown() {
  delete fragmentation_;
}

// First octet tests.
#define EXPECT_BIT_EQ(x, n, a) EXPECT_EQ((((x) >> (n)) & 0x1), a)

#define EXPECT_RSV_ZERO(x) EXPECT_EQ(((x) & 0xE0), 0)

#define EXPECT_BIT_X_EQ(x, a) EXPECT_BIT_EQ(x, 7, a)

#define EXPECT_BIT_N_EQ(x, a) EXPECT_BIT_EQ(x, 5, a)

#define EXPECT_BIT_S_EQ(x, a) EXPECT_BIT_EQ(x, 4, a)

#define EXPECT_PART_ID_EQ(x, a) EXPECT_EQ(((x) & 0x0F), a)

// Extension fields tests
#define EXPECT_BIT_I_EQ(x, a) EXPECT_BIT_EQ(x, 7, a)

#define EXPECT_BIT_L_EQ(x, a) EXPECT_BIT_EQ(x, 6, a)

#define EXPECT_BIT_T_EQ(x, a) EXPECT_BIT_EQ(x, 5, a)

#define EXPECT_BIT_K_EQ(x, a) EXPECT_BIT_EQ(x, 4, a)

#define EXPECT_TID_EQ(x, a) EXPECT_EQ((((x) & 0xC0) >> 6), a)

#define EXPECT_BIT_Y_EQ(x, a) EXPECT_BIT_EQ(x, 5, a)

#define EXPECT_KEYIDX_EQ(x, a) EXPECT_EQ(((x) & 0x1F), a)

void RtpFormatVp8Test::CheckHeader(bool first_in_frame, bool frag_start,
                                   int part_id) {
  payload_start_ = 1;
  EXPECT_BIT_EQ(buffer_[0], 6, 0);  // Check reserved bit.


  if (hdr_info_.pictureId != kNoPictureId ||
      hdr_info_.temporalIdx != kNoTemporalIdx ||
      hdr_info_.tl0PicIdx != kNoTl0PicIdx ||
      hdr_info_.keyIdx != kNoKeyIdx) {
    EXPECT_BIT_X_EQ(buffer_[0], 1);
    ++payload_start_;
    CheckPictureID();
    CheckTl0PicIdx();
    CheckTIDAndKeyIdx();
  } else {
    EXPECT_BIT_X_EQ(buffer_[0], 0);
  }

  EXPECT_BIT_N_EQ(buffer_[0], 0);
  EXPECT_BIT_S_EQ(buffer_[0], frag_start);

  // Check partition index.
  if (part_id < 0) {
    // (Payload data is the same as the partition index.)
    EXPECT_EQ(buffer_[0] & 0x0F, buffer_[payload_start_]);
  } else {
    EXPECT_EQ(buffer_[0] & 0x0F, part_id);
  }
}

void RtpFormatVp8Test::CheckPictureID() {
  if (hdr_info_.pictureId != kNoPictureId) {
    EXPECT_BIT_I_EQ(buffer_[1], 1);
    if (hdr_info_.pictureId > 0x7F) {
      EXPECT_BIT_EQ(buffer_[payload_start_], 7, 1);
      EXPECT_EQ(buffer_[payload_start_] & 0x7F,
                (hdr_info_.pictureId >> 8) & 0x7F);
      EXPECT_EQ(buffer_[payload_start_ + 1],
                hdr_info_.pictureId & 0xFF);
      payload_start_ += 2;
    } else {
      EXPECT_BIT_EQ(buffer_[payload_start_], 7, 0);
      EXPECT_EQ(buffer_[payload_start_] & 0x7F,
                (hdr_info_.pictureId) & 0x7F);
      payload_start_ += 1;
    }
  } else {
    EXPECT_BIT_I_EQ(buffer_[1], 0);
  }
}

void RtpFormatVp8Test::CheckTl0PicIdx() {
  if (hdr_info_.tl0PicIdx != kNoTl0PicIdx) {
    EXPECT_BIT_L_EQ(buffer_[1], 1);
    EXPECT_EQ(buffer_[payload_start_], hdr_info_.tl0PicIdx);
    ++payload_start_;
  } else {
    EXPECT_BIT_L_EQ(buffer_[1], 0);
  }
}

void RtpFormatVp8Test::CheckTIDAndKeyIdx() {
  if (hdr_info_.temporalIdx == kNoTemporalIdx &&
      hdr_info_.keyIdx == kNoKeyIdx) {
    EXPECT_BIT_T_EQ(buffer_[1], 0);
    EXPECT_BIT_K_EQ(buffer_[1], 0);
    return;
  }
  if (hdr_info_.temporalIdx != kNoTemporalIdx) {
    EXPECT_BIT_T_EQ(buffer_[1], 1);
    EXPECT_TID_EQ(buffer_[payload_start_], hdr_info_.temporalIdx);
    EXPECT_BIT_Y_EQ(buffer_[payload_start_], hdr_info_.layerSync);
  } else {
    EXPECT_BIT_T_EQ(buffer_[1], 0);
    EXPECT_TID_EQ(buffer_[payload_start_], 0);
    EXPECT_BIT_Y_EQ(buffer_[payload_start_], false);
  }
  if (hdr_info_.keyIdx != kNoKeyIdx) {
    EXPECT_BIT_K_EQ(buffer_[1], 1);
    EXPECT_KEYIDX_EQ(buffer_[payload_start_], hdr_info_.keyIdx);
  } else {
    EXPECT_BIT_K_EQ(buffer_[1], 0);
    EXPECT_KEYIDX_EQ(buffer_[payload_start_], 0);
  }
  ++payload_start_;
}

void RtpFormatVp8Test::CheckPayload(int payload_end) {
  for (int i = payload_start_; i < payload_end; i++, data_ptr_++)
    EXPECT_EQ(buffer_[i], *data_ptr_);
}

void RtpFormatVp8Test::CheckLast(bool last) const {
  EXPECT_EQ(last, data_ptr_ == payload_data_ + kPayloadSize);
}

void RtpFormatVp8Test::CheckPacket(int send_bytes, int expect_bytes, bool last,
                                   bool first_in_frame, bool frag_start) {
  EXPECT_EQ(send_bytes, expect_bytes);
  CheckHeader(first_in_frame, frag_start, -1);
  CheckPayload(send_bytes);
  CheckLast(last);
}

void RtpFormatVp8Test::CheckPacketZeroPartId(int send_bytes,
                                             int expect_bytes,
                                             bool last,
                                             bool first_in_frame,
                                             bool frag_start) {
  EXPECT_EQ(send_bytes, expect_bytes);
  CheckHeader(first_in_frame, frag_start, 0);
  CheckPayload(send_bytes);
  CheckLast(last);
}

TEST_F(RtpFormatVp8Test, TestStrictMode) {
  int send_bytes = 0;
  bool last;
  bool first_in_frame = true;

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_, kStrict);

  // Get first packet, expect balanced size ~= same as second packet.
  EXPECT_EQ(0, packetizer.NextPacket(13, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 8, last,
              first_in_frame,
              /* frag_start */ true);
  first_in_frame = false;

  // Get second packet.
  EXPECT_EQ(0, packetizer.NextPacket(13, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 10, last,
              first_in_frame,
              /* frag_start */ false);

  // Second partition.
  // Get first (and only) packet.
  EXPECT_EQ(1, packetizer.NextPacket(20, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 14, last,
              first_in_frame,
              /* frag_start */ true);

  // Third partition.
  // Get first packet (of four).
  EXPECT_EQ(2, packetizer.NextPacket(7, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 5, last,
              first_in_frame,
              /* frag_start */ true);

  // Get second packet (of four).
  EXPECT_EQ(2, packetizer.NextPacket(7, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 5, last,
              first_in_frame,
              /* frag_start */ false);

  // Get third packet (of four).
  EXPECT_EQ(2, packetizer.NextPacket(7, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 7, last,
              first_in_frame,
              /* frag_start */ false);

  // Get fourth and last packet.
  EXPECT_EQ(2, packetizer.NextPacket(7, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 5, last,
              first_in_frame,
              /* frag_start */ false);
}

TEST_F(RtpFormatVp8Test, TestAggregateMode) {
  int send_bytes = 0;
  bool last;
  bool first_in_frame = true;

  hdr_info_.pictureId = 20;  // <= 0x7F should produce 1-byte PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_,
                                         kAggregate);

  // Get first packet.
  // First part of first partition (balanced fragments are expected).
  EXPECT_EQ(0, packetizer.NextPacket(8, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 7, last,
              first_in_frame,
              /* frag_start */ true);
  first_in_frame = false;

  // Get second packet.
  // Second fragment of first partition.
  EXPECT_EQ(0, packetizer.NextPacket(8, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 5, last,
              first_in_frame,
              /* frag_start */ false);

  // Get third packet.
  // Third fragment of first partition.
  EXPECT_EQ(0, packetizer.NextPacket(8, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 7, last,
              first_in_frame,
              /* frag_start */ false);

  // Get fourth packet.
  // Last two partitions aggregated.
  EXPECT_EQ(1, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 23, last,
              first_in_frame,
              /* frag_start */ true);
}

TEST_F(RtpFormatVp8Test, TestSloppyMode) {
  int send_bytes = 0;
  bool last;
  bool first_in_frame = true;

  hdr_info_.pictureId = kNoPictureId;  // No PictureID.
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_, kSloppy);

  // Get first packet.
  EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 9, last,
              first_in_frame,
              /* frag_start */ true);
  first_in_frame = false;

  // Get second packet.
  // Fragments of first and second partitions.
  EXPECT_EQ(0, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 9, last,
              first_in_frame,
              /* frag_start */ false);

  // Get third packet.
  // Fragments of second and third partitions.
  EXPECT_EQ(1, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 9, last,
              first_in_frame,
              /* frag_start */ false);

  // Get fourth packet.
  // Second half of last partition.
  EXPECT_EQ(2, packetizer.NextPacket(9, buffer_, &send_bytes, &last));
  CheckPacket(send_bytes, 7, last,
              first_in_frame,
              /* frag_start */ false);
}

// Verify that sloppy mode is forced if fragmentation info is missing.
TEST_F(RtpFormatVp8Test, TestSloppyModeFallback) {
  int send_bytes = 0;
  bool last;
  bool first_in_frame = true;

  hdr_info_.pictureId = 200;  // > 0x7F should produce 2-byte PictureID
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_);

  // Get first packet.
  EXPECT_EQ(0, packetizer.NextPacket(10, buffer_, &send_bytes, &last));
  CheckPacketZeroPartId(send_bytes, 10, last,
                        first_in_frame,
                        /* frag_start */ true);
  first_in_frame = false;

  // Get second packet.
  // Fragments of first and second partitions.
  EXPECT_EQ(0, packetizer.NextPacket(10, buffer_, &send_bytes, &last));
  CheckPacketZeroPartId(send_bytes, 10, last,
                        first_in_frame,
                        /* frag_start */ false);

  // Get third packet.
  // Fragments of second and third partitions.
  EXPECT_EQ(0, packetizer.NextPacket(10, buffer_, &send_bytes, &last));
  CheckPacketZeroPartId(send_bytes, 10, last,
                        first_in_frame,
                        /* frag_start */ false);

  // Get fourth packet.
  // Second half of last partition.
  EXPECT_EQ(0, packetizer.NextPacket(7, buffer_, &send_bytes, &last));
  CheckPacketZeroPartId(send_bytes, 7, last,
                        first_in_frame,
                        /* frag_start */ false);
}

// Verify that non-reference bit is set.
TEST_F(RtpFormatVp8Test, TestNonReferenceBit) {
  int send_bytes = 0;
  bool last;

  hdr_info_.nonReference = true;
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_);

  // Get first packet.
  ASSERT_EQ(0, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
  ASSERT_FALSE(last);
  EXPECT_BIT_N_EQ(buffer_[0], 1);

  // Get second packet.
  ASSERT_EQ(0, packetizer.NextPacket(25, buffer_, &send_bytes, &last));
  ASSERT_TRUE(last);
  EXPECT_BIT_N_EQ(buffer_[0], 1);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST_F(RtpFormatVp8Test, TestTl0PicIdxAndTID) {
  int send_bytes = 0;
  bool last;

  hdr_info_.tl0PicIdx = 117;
  hdr_info_.temporalIdx = 2;
  hdr_info_.layerSync = true;
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_,
                                         kAggregate);

  // Get first and only packet.
  EXPECT_EQ(0, packetizer.NextPacket(kBufferSize, buffer_, &send_bytes,
                                     &last));
  bool first_in_frame = true;
  CheckPacket(send_bytes, kPayloadSize + 4, last,
              first_in_frame,
              /* frag_start */ true);
}

// Verify KeyIdx field.
TEST_F(RtpFormatVp8Test, TestKeyIdx) {
  int send_bytes = 0;
  bool last;

  hdr_info_.keyIdx = 17;
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_,
                                         kAggregate);

  // Get first and only packet.
  EXPECT_EQ(0, packetizer.NextPacket(kBufferSize, buffer_, &send_bytes,
                                     &last));
  bool first_in_frame = true;
  CheckPacket(send_bytes, kPayloadSize + 3, last,
              first_in_frame,
              /* frag_start */ true);
}

// Verify TID field and KeyIdx field in combination.
TEST_F(RtpFormatVp8Test, TestTIDAndKeyIdx) {
  int send_bytes = 0;
  bool last;

  hdr_info_.temporalIdx = 1;
  hdr_info_.keyIdx = 5;
  RtpFormatVp8 packetizer = RtpFormatVp8(payload_data_, kPayloadSize,
                                         hdr_info_, *fragmentation_,
                                         kAggregate);

  // Get first and only packet.
  EXPECT_EQ(0, packetizer.NextPacket(kBufferSize, buffer_, &send_bytes,
                                     &last));
  bool first_in_frame = true;
  CheckPacket(send_bytes, kPayloadSize + 3, last,
              first_in_frame,
              /* frag_start */ true);
}

}  // namespace
