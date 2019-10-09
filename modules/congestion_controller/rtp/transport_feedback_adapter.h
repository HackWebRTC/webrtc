/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_

#include <deque>
#include <map>
#include <utility>
#include <vector>

#include "api/transport/network_types.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class PacketFeedbackObserver;
struct RtpPacketSendInfo;

namespace rtcp {
class TransportFeedback;
}  // namespace rtcp

class TransportFeedbackAdapter {
 public:
  TransportFeedbackAdapter();
  virtual ~TransportFeedbackAdapter();

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  void AddPacket(const RtpPacketSendInfo& packet_info,
                 size_t overhead_bytes,
                 Timestamp creation_time);
  absl::optional<SentPacket> ProcessSentPacket(
      const rtc::SentPacket& sent_packet);

  absl::optional<TransportPacketsFeedback> ProcessTransportFeedback(
      const rtcp::TransportFeedback& feedback,
      Timestamp feedback_time);

  std::vector<PacketFeedback> GetTransportFeedbackVector() const;

  void SetNetworkIds(uint16_t local_id, uint16_t remote_id);

  DataSize GetOutstandingData() const;

 private:
  using RemoteAndLocalNetworkId = std::pair<uint16_t, uint16_t>;

  enum class SendTimeHistoryStatus { kNotAdded, kOk, kDuplicate };

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback);

  std::vector<PacketFeedback> GetPacketFeedbackVector(
      const rtcp::TransportFeedback& feedback,
      Timestamp feedback_time);

  // Look up PacketFeedback for a sent packet, based on the sequence number, and
  // populate all fields except for arrival_time. The packet parameter must
  // thus be non-null and have the sequence_number field set.
  bool GetFeedback(PacketFeedback* packet_feedback, bool remove)
      RTC_RUN_ON(&lock_);
  void AddInFlightPacketBytes(const PacketFeedback& packet) RTC_RUN_ON(&lock_);
  void RemoveInFlightPacketBytes(const PacketFeedback& packet)
      RTC_RUN_ON(&lock_);

  rtc::CriticalSection lock_;

  const int64_t packet_age_limit_ms_;
  size_t pending_untracked_size_ RTC_GUARDED_BY(&lock_) = 0;
  int64_t last_send_time_ms_ RTC_GUARDED_BY(&lock_) = -1;
  int64_t last_untracked_send_time_ms_ RTC_GUARDED_BY(&lock_) = -1;
  SequenceNumberUnwrapper seq_num_unwrapper_ RTC_GUARDED_BY(&lock_);
  std::map<int64_t, PacketFeedback> history_ RTC_GUARDED_BY(&lock_);

  // Sequence numbers are never negative, using -1 as it always < a real
  // sequence number.
  int64_t last_ack_seq_num_ RTC_GUARDED_BY(&lock_) = -1;
  std::map<RemoteAndLocalNetworkId, size_t> in_flight_bytes_
      RTC_GUARDED_BY(&lock_);

  int64_t current_offset_ms_;
  int64_t last_timestamp_us_;
  std::vector<PacketFeedback> last_packet_feedback_vector_;
  uint16_t local_net_id_ RTC_GUARDED_BY(&lock_);
  uint16_t remote_net_id_ RTC_GUARDED_BY(&lock_);

  rtc::CriticalSection observers_lock_;
  std::vector<PacketFeedbackObserver*> observers_
      RTC_GUARDED_BY(&observers_lock_);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_
