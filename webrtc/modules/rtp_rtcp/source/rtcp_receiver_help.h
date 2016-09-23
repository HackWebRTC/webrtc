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
#include <vector>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/typedefs.h"

namespace webrtc {
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
