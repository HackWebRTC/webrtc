/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace rtcp {
class TransportFeedback;
}
namespace RTCPHelp {

class RTCPReportBlockInformation {
 public:
  RTCPReportBlockInformation();
  ~RTCPReportBlockInformation();

  // Statistics
  RTCPReportBlock remoteReceiveBlock;
  uint32_t remoteMaxJitter;

  // RTT
  int64_t RTT;
  int64_t minRTT;
  int64_t maxRTT;
  int64_t avgRTT;
  uint32_t numAverageCalcs;
};

class RTCPPacketInformation {
 public:
  RTCPPacketInformation();
  ~RTCPPacketInformation();

  void AddNACKPacket(const uint16_t packetID);
  void ResetNACKPacketIdArray();

  void AddReportInfo(const RTCPReportBlockInformation& report_block_info);

  uint32_t rtcpPacketTypeFlags;  // RTCPPacketTypeFlags bit field
  uint32_t remoteSSRC;

  std::vector<uint16_t> nackSequenceNumbers;

  ReportBlockList report_blocks;
  int64_t rtt;

  uint8_t sliPictureId;
  uint64_t rpsiPictureId;
  uint32_t receiverEstimatedMaxBitrate;

  uint32_t ntp_secs;
  uint32_t ntp_frac;
  uint32_t rtp_timestamp;

  uint32_t xr_originator_ssrc;
  bool xr_dlrr_item;

  std::unique_ptr<rtcp::TransportFeedback> transport_feedback_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(RTCPPacketInformation);
};

class RTCPReceiveInformation {
 public:
  RTCPReceiveInformation();
  ~RTCPReceiveInformation();

  void InsertTmmbrItem(uint32_t sender_ssrc,
                       const rtcp::TmmbItem& tmmbr_item,
                       int64_t current_time_ms);

  void GetTmmbrSet(int64_t current_time_ms,
                   std::vector<rtcp::TmmbItem>* candidates);

  void ClearTmmbr();

  int64_t last_time_received_ms = 0;

  int32_t last_fir_sequence_number = -1;
  int64_t last_fir_request_ms = 0;

  bool ready_for_delete = false;

  std::vector<rtcp::TmmbItem> tmmbn;

 private:
  struct TimedTmmbrItem {
    rtcp::TmmbItem tmmbr_item;
    int64_t last_updated_ms;
  };

  std::map<uint32_t, TimedTmmbrItem> tmmbr_;
};

}  // end namespace RTCPHelp
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_HELP_H_
