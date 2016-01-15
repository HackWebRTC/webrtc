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

using webrtc::RTCPUtility::PT_XR;
using webrtc::RTCPUtility::RTCPPacketXR;

namespace webrtc {
namespace rtcp {
namespace {
void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 4;
}
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
void CreateXrHeader(const RTCPPacketXR& header,
                    uint8_t* buffer,
                    size_t* pos) {
  AssignUWord32(buffer, pos, header.OriginatorSSRC);
}
}  // namespace

bool ExtendedReports::Create(uint8_t* packet,
                size_t* index,
                size_t max_length,
                RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(0U, PT_XR, HeaderLength(), packet, index);
  CreateXrHeader(xr_header_, packet, index);
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
  return true;
}

bool ExtendedReports::WithRrtr(Rrtr* rrtr) {
  RTC_DCHECK(rrtr);
  if (rrtr_blocks_.size() >= kMaxNumberOfRrtrBlocks) {
    LOG(LS_WARNING) << "Max RRTR blocks reached.";
    return false;
  }
  rrtr_blocks_.push_back(*rrtr);
  return true;
}

bool ExtendedReports::WithDlrr(Dlrr* dlrr) {
  RTC_DCHECK(dlrr);
  if (dlrr_blocks_.size() >= kMaxNumberOfDlrrBlocks) {
    LOG(LS_WARNING) << "Max DLRR blocks reached.";
    return false;
  }
  dlrr_blocks_.push_back(*dlrr);
  return true;
}

bool ExtendedReports::WithVoipMetric(VoipMetric* voip_metric) {
  assert(voip_metric);
  if (voip_metric_blocks_.size() >= kMaxNumberOfVoipMetricBlocks) {
    LOG(LS_WARNING) << "Max Voip Metric blocks reached.";
    return false;
  }
  voip_metric_blocks_.push_back(*voip_metric);
  return true;
}

size_t ExtendedReports::DlrrLength() const {
  size_t length = 0;
  for (const Dlrr& block : dlrr_blocks_) {
    length += block.BlockLength();
  }
  return length;
}

}  // namespace rtcp
}  // namespace webrtc
