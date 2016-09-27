/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t ExtendedReports::kPacketType;
// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
//
// Format for XR packets:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|reserved |   PT=XR=207   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              SSRC                             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :                         report blocks                         :
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Extended report block:
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  Block Type   |   reserved    |         block length          |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :             type-specific block contents                      :
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
ExtendedReports::ExtendedReports() : sender_ssrc_(0) {}
ExtendedReports::~ExtendedReports() {}

bool ExtendedReports::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);

  if (packet.payload_size_bytes() < kXrBaseLength) {
    LOG(LS_WARNING) << "Packet is too small to be an ExtendedReports packet.";
    return false;
  }

  sender_ssrc_ = ByteReader<uint32_t>::ReadBigEndian(packet.payload());
  rrtr_blocks_.clear();
  dlrr_blocks_.clear();
  voip_metric_blocks_.clear();

  const uint8_t* current_block = packet.payload() + kXrBaseLength;
  const uint8_t* const packet_end =
      packet.payload() + packet.payload_size_bytes();
  constexpr size_t kBlockHeaderSizeBytes = 4;
  while (current_block + kBlockHeaderSizeBytes <= packet_end) {
    uint8_t block_type = ByteReader<uint8_t>::ReadBigEndian(current_block);
    uint16_t block_length =
        ByteReader<uint16_t>::ReadBigEndian(current_block + 2);
    const uint8_t* next_block =
        current_block + kBlockHeaderSizeBytes + block_length * 4;
    if (next_block > packet_end) {
      LOG(LS_WARNING) << "Report block in extended report packet is too big.";
      return false;
    }
    switch (block_type) {
      case Rrtr::kBlockType:
        ParseRrtrBlock(current_block, block_length);
        break;
      case Dlrr::kBlockType:
        ParseDlrrBlock(current_block, block_length);
        break;
      case VoipMetric::kBlockType:
        ParseVoipMetricBlock(current_block, block_length);
        break;
      default:
        // Unknown block, ignore.
        LOG(LS_WARNING) << "Unknown extended report block type " << block_type;
        break;
    }
    current_block = next_block;
  }

  return true;
}

bool ExtendedReports::WithRrtr(const Rrtr& rrtr) {
  if (rrtr_blocks_.size() >= kMaxNumberOfRrtrBlocks) {
    LOG(LS_WARNING) << "Max RRTR blocks reached.";
    return false;
  }
  rrtr_blocks_.push_back(rrtr);
  return true;
}

bool ExtendedReports::WithDlrr(const Dlrr& dlrr) {
  if (dlrr_blocks_.size() >= kMaxNumberOfDlrrBlocks) {
    LOG(LS_WARNING) << "Max DLRR blocks reached.";
    return false;
  }
  dlrr_blocks_.push_back(dlrr);
  return true;
}

bool ExtendedReports::WithVoipMetric(const VoipMetric& voip_metric) {
  if (voip_metric_blocks_.size() >= kMaxNumberOfVoipMetricBlocks) {
    LOG(LS_WARNING) << "Max Voip Metric blocks reached.";
    return false;
  }
  voip_metric_blocks_.push_back(voip_metric);
  return true;
}

bool ExtendedReports::Create(uint8_t* packet,
                             size_t* index,
                             size_t max_length,
                             RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  size_t index_end = *index + BlockLength();
  const uint8_t kReserved = 0;
  CreateHeader(kReserved, kPacketType, HeaderLength(), packet, index);
  ByteWriter<uint32_t>::WriteBigEndian(packet + *index, sender_ssrc_);
  *index += sizeof(uint32_t);
  for (const Rrtr& block : rrtr_blocks_) {
    block.Create(packet + *index);
    *index += Rrtr::kLength;
  }
  for (const Dlrr& block : dlrr_blocks_) {
    block.Create(packet + *index);
    *index += block.BlockLength();
  }
  for (const VoipMetric& block : voip_metric_blocks_) {
    block.Create(packet + *index);
    *index += VoipMetric::kLength;
  }
  RTC_CHECK_EQ(*index, index_end);
  return true;
}

size_t ExtendedReports::DlrrLength() const {
  size_t length = 0;
  for (const Dlrr& block : dlrr_blocks_) {
    length += block.BlockLength();
  }
  return length;
}

void ExtendedReports::ParseRrtrBlock(const uint8_t* block,
                                     uint16_t block_length) {
  if (block_length != Rrtr::kBlockLength) {
    LOG(LS_WARNING) << "Incorrect rrtr block size " << block_length
                    << " Should be " << Rrtr::kBlockLength;
    return;
  }
  rrtr_blocks_.push_back(Rrtr());
  rrtr_blocks_.back().Parse(block);
}

void ExtendedReports::ParseDlrrBlock(const uint8_t* block,
                                     uint16_t block_length) {
  dlrr_blocks_.push_back(Dlrr());
  if (!dlrr_blocks_.back().Parse(block, block_length)) {
    dlrr_blocks_.pop_back();
  }
}

void ExtendedReports::ParseVoipMetricBlock(const uint8_t* block,
                                           uint16_t block_length) {
  if (block_length != VoipMetric::kBlockLength) {
    LOG(LS_WARNING) << "Incorrect voip metric block size " << block_length
                    << " Should be " << VoipMetric::kBlockLength;
    return;
  }
  voip_metric_blocks_.push_back(VoipMetric());
  voip_metric_blocks_.back().Parse(block);
}
}  // namespace rtcp
}  // namespace webrtc
