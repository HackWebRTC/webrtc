/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver_help.h"

#include <assert.h>  // assert
#include <string.h>  // memset

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
namespace RTCPHelp {

RTCPPacketInformation::RTCPPacketInformation()
    : rtcpPacketTypeFlags(0),
      remoteSSRC(0),
      nackSequenceNumbers(),
      rtt(0),
      sliPictureId(0),
      rpsiPictureId(0),
      receiverEstimatedMaxBitrate(0),
      ntp_secs(0),
      ntp_frac(0),
      rtp_timestamp(0),
      xr_originator_ssrc(0),
      xr_dlrr_item(false) {}

RTCPPacketInformation::~RTCPPacketInformation() {}

void RTCPPacketInformation::ResetNACKPacketIdArray() {
  nackSequenceNumbers.clear();
}

void RTCPPacketInformation::AddNACKPacket(const uint16_t packetID) {
  if (nackSequenceNumbers.size() >= kSendSideNackListSizeSanity) {
    return;
  }
  nackSequenceNumbers.push_back(packetID);
}

void RTCPPacketInformation::AddReportInfo(
    const RTCPReportBlockInformation& report_block_info) {
  this->rtt = report_block_info.RTT;
  report_blocks.push_back(report_block_info.remoteReceiveBlock);
}

RTCPReportBlockInformation::RTCPReportBlockInformation()
    : remoteReceiveBlock(),
      remoteMaxJitter(0),
      RTT(0),
      minRTT(0),
      maxRTT(0),
      avgRTT(0),
      numAverageCalcs(0) {
  memset(&remoteReceiveBlock, 0, sizeof(remoteReceiveBlock));
}

RTCPReportBlockInformation::~RTCPReportBlockInformation() {}

RTCPReceiveInformation::RTCPReceiveInformation() = default;
RTCPReceiveInformation::~RTCPReceiveInformation() = default;

void RTCPReceiveInformation::InsertTmmbrItem(uint32_t sender_ssrc,
                                             const rtcp::TmmbItem& tmmbr_item,
                                             int64_t current_time_ms) {
  TimedTmmbrItem* entry = &tmmbr_[sender_ssrc];
  entry->tmmbr_item = rtcp::TmmbItem(sender_ssrc, tmmbr_item.bitrate_bps(),
                                     tmmbr_item.packet_overhead());
  entry->last_updated_ms = current_time_ms;
}

void RTCPReceiveInformation::GetTmmbrSet(
    int64_t current_time_ms,
    std::vector<rtcp::TmmbItem>* candidates) {
  // Use audio define since we don't know what interval the remote peer use.
  int64_t timeouted_ms = current_time_ms - 5 * RTCP_INTERVAL_AUDIO_MS;
  for (auto it = tmmbr_.begin(); it != tmmbr_.end();) {
    if (it->second.last_updated_ms < timeouted_ms) {
      // Erase timeout entries.
      it = tmmbr_.erase(it);
    } else {
      candidates->push_back(it->second.tmmbr_item);
      ++it;
    }
  }
}

void RTCPReceiveInformation::ClearTmmbr() {
  tmmbr_.clear();
}

}  // namespace RTCPHelp
}  // namespace webrtc
