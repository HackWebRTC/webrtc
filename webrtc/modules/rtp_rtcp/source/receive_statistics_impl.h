/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

#include <algorithm>

#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;

class ReceiveStatisticsImpl : public ReceiveStatistics {
 public:
  explicit ReceiveStatisticsImpl(Clock* clock);

  // Implements ReceiveStatistics.
  void IncomingPacket(const RTPHeader& header, size_t bytes,
                      bool old_packet, bool in_order);
  bool Statistics(RtpReceiveStatistics* statistics, bool reset);
  bool Statistics(RtpReceiveStatistics* statistics, int32_t* missing,
                  bool reset);
  void GetDataCounters(uint32_t* bytes_received,
                       uint32_t* packets_received) const;
  uint32_t BitrateReceived();
  void ResetStatistics();
  void ResetDataCounters();

  // Implements Module.
  int32_t TimeUntilNextProcess();
  int32_t Process();

 private:
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  Clock* clock_;
  Bitrate incoming_bitrate_;
  uint32_t ssrc_;
  // Stats on received RTP packets.
  uint32_t jitter_q4_;
  uint32_t jitter_max_q4_;
  uint32_t cumulative_loss_;
  uint32_t jitter_q4_transmission_time_offset_;

  uint32_t local_time_last_received_timestamp_;
  uint32_t last_received_timestamp_;
  int32_t last_received_transmission_time_offset_;
  uint16_t received_seq_first_;
  uint16_t received_seq_max_;
  uint16_t received_seq_wraps_;

  // Current counter values.
  uint16_t received_packet_overhead_;
  uint32_t received_byte_count_;
  uint32_t received_retransmitted_packets_;
  uint32_t received_inorder_packet_count_;

  // Counter values when we sent the last report.
  uint32_t last_report_inorder_packets_;
  uint32_t last_report_old_packets_;
  uint16_t last_report_seq_max_;
  RtpReceiveStatistics last_reported_statistics_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
