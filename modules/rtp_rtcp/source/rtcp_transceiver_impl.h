/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/optional.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_config.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/weak_ptr.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is not thread-safe.
class RtcpTransceiverImpl {
 public:
  explicit RtcpTransceiverImpl(const RtcpTransceiverConfig& config);
  ~RtcpTransceiverImpl();

  // Handles incoming rtcp packets.
  void ReceivePacket(rtc::ArrayView<const uint8_t> packet);

  // Sends RTCP packets starting with a sender or receiver report.
  void SendCompoundPacket();

  // (REMB) Receiver Estimated Max Bitrate.
  // Includes REMB in following compound packets.
  void SetRemb(int bitrate_bps, std::vector<uint32_t> ssrcs);
  // Stops sending REMB in following compound packets.
  void UnsetRemb();

 private:
  struct SenderReportTimes {
    int64_t local_received_time_us;
    NtpTime remote_sent_time;
  };

  void HandleReceivedPacket(const rtcp::CommonHeader& rtcp_packet_header);

  void ReschedulePeriodicCompoundPackets(int64_t delay_ms);
  // Sends RTCP packets.
  void SendPacket();
  // Generate Report Blocks to be send in Sender or Receiver Report.
  std::vector<rtcp::ReportBlock> CreateReportBlocks();

  const RtcpTransceiverConfig config_;

  rtc::Optional<rtcp::Remb> remb_;
  std::map<uint32_t, SenderReportTimes> last_received_sender_reports_;
  rtc::WeakPtrFactory<RtcpTransceiverImpl> ptr_factory_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiverImpl);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
