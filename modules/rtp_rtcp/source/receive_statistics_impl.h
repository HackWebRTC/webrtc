/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include <algorithm>
#include <map>
#include <vector>

#include "absl/types/optional.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class StreamStatisticianImpl : public StreamStatistician,
                               public RtpPacketSinkInterface {
 public:
  StreamStatisticianImpl(uint32_t ssrc,
                         Clock* clock,
                         int max_reordering_threshold);
  ~StreamStatisticianImpl() override;

  RtpReceiveStats GetStats() const override;

  // |reset| here and in next method restarts calculation of fraction_lost stat.
  bool GetStatistics(RtcpStatistics* statistics, bool reset) override;
  bool GetActiveStatisticsAndReset(RtcpStatistics* statistics);
  absl::optional<int> GetFractionLostInPercent() const override;
  StreamDataCounters GetReceiveStreamDataCounters() const override;
  uint32_t BitrateReceived() const override;

  // Implements RtpPacketSinkInterface
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  void SetMaxReorderingThreshold(int max_reordering_threshold);
  void EnableRetransmitDetection(bool enable);

 private:
  bool IsRetransmitOfOldPacket(const RtpPacketReceived& packet,
                               int64_t now_ms) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  RtcpStatistics CalculateRtcpStatistics()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  void UpdateJitter(const RtpPacketReceived& packet, int64_t receive_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  // Updates StreamStatistician for out of order packets.
  // Returns true if packet considered to be out of order.
  bool UpdateOutOfOrder(const RtpPacketReceived& packet,
                        int64_t sequence_number,
                        int64_t now_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  // Updates StreamStatistician for incoming packets.
  StreamDataCounters UpdateCounters(const RtpPacketReceived& packet);
  // Checks if this StreamStatistician received any rtp packets.
  bool ReceivedRtpPacket() const RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_) {
    return received_seq_max_ >= 0;
  }

  const uint32_t ssrc_;
  Clock* const clock_;
  rtc::CriticalSection stream_lock_;
  RateStatistics incoming_bitrate_ RTC_GUARDED_BY(&stream_lock_);
  // In number of packets or sequence numbers.
  int max_reordering_threshold_ RTC_GUARDED_BY(&stream_lock_);
  bool enable_retransmit_detection_ RTC_GUARDED_BY(&stream_lock_);

  // Stats on received RTP packets.
  uint32_t jitter_q4_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t cumulative_loss_ RTC_GUARDED_BY(&stream_lock_);

  int64_t last_receive_time_ms_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t last_received_timestamp_ RTC_GUARDED_BY(&stream_lock_);
  SequenceNumberUnwrapper seq_unwrapper_ RTC_GUARDED_BY(&stream_lock_);
  int64_t received_seq_first_ RTC_GUARDED_BY(&stream_lock_);
  int64_t received_seq_max_ RTC_GUARDED_BY(&stream_lock_);
  // Assume that the other side restarted when there are two sequential packets
  // with large jump from received_seq_max_.
  absl::optional<uint16_t> received_seq_out_of_order_
      RTC_GUARDED_BY(&stream_lock_);

  // Current counter values.
  StreamDataCounters receive_counters_ RTC_GUARDED_BY(&stream_lock_);

  // Counter values when we sent the last report.
  uint32_t last_report_inorder_packets_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t last_report_old_packets_ RTC_GUARDED_BY(&stream_lock_);
  int64_t last_report_seq_max_ RTC_GUARDED_BY(&stream_lock_);
  RtcpStatistics last_reported_statistics_ RTC_GUARDED_BY(&stream_lock_);
};

class ReceiveStatisticsImpl : public ReceiveStatistics {
 public:
  explicit ReceiveStatisticsImpl(Clock* clock);

  ~ReceiveStatisticsImpl() override;

  // Implements ReceiveStatisticsProvider.
  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override;

  // Implements RtpPacketSinkInterface
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Implements ReceiveStatistics.
  // Note: More specific return type for use in the implementation.
  StreamStatisticianImpl* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void SetMaxReorderingThreshold(uint32_t ssrc,
                                 int max_reordering_threshold) override;
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override;

 private:
  StreamStatisticianImpl* GetOrCreateStatistician(uint32_t ssrc);

  Clock* const clock_;
  rtc::CriticalSection receive_statistics_lock_;
  uint32_t last_returned_ssrc_;
  int max_reordering_threshold_ RTC_GUARDED_BY(receive_statistics_lock_);
  std::map<uint32_t, StreamStatisticianImpl*> statisticians_
      RTC_GUARDED_BY(receive_statistics_lock_);
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
