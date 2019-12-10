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

struct PacketFeedback {
  PacketFeedback() = default;
  // Time corresponding to when this object was created.
  Timestamp creation_time = Timestamp::MinusInfinity();
  SentPacket sent;
  // Time corresponding to when the packet was received. Timestamped with the
  // receiver's clock. For unreceived packet, Timestamp::PlusInfinity() is used.
  Timestamp receive_time = Timestamp::PlusInfinity();

  // The network route ids that this packet is associated with.
  uint16_t local_net_id = 0;
  uint16_t remote_net_id = 0;
  // The SSRC and RTP sequence number of the packet this feedback refers to.
  absl::optional<uint32_t> ssrc;
  uint16_t rtp_sequence_number = 0;
};

class InFlightBytesTracker {
 public:
  void AddInFlightPacketBytes(const PacketFeedback& packet);
  void RemoveInFlightPacketBytes(const PacketFeedback& packet);
  DataSize GetOutstandingData(uint16_t local_net_id,
                              uint16_t remote_net_id) const;

 private:
  using RemoteAndLocalNetworkId = std::pair<uint16_t, uint16_t>;
  std::map<RemoteAndLocalNetworkId, DataSize> in_flight_data_;
};

class TransportFeedbackAdapter : public StreamFeedbackProvider {
 public:
  TransportFeedbackAdapter();
  virtual ~TransportFeedbackAdapter();

  void RegisterStreamFeedbackObserver(
      std::vector<uint32_t> ssrcs,
      StreamFeedbackObserver* observer) override;
  void DeRegisterStreamFeedbackObserver(
      StreamFeedbackObserver* observer) override;

  void AddPacket(const RtpPacketSendInfo& packet_info,
                 size_t overhead_bytes,
                 Timestamp creation_time);
  absl::optional<SentPacket> ProcessSentPacket(
      const rtc::SentPacket& sent_packet);

  absl::optional<TransportPacketsFeedback> ProcessTransportFeedback(
      const rtcp::TransportFeedback& feedback,
      Timestamp feedback_time);

  void SetNetworkIds(uint16_t local_id, uint16_t remote_id);

  DataSize GetOutstandingData() const;

 private:
  enum class SendTimeHistoryStatus { kNotAdded, kOk, kDuplicate };

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback);

  std::vector<PacketFeedback> ProcessTransportFeedbackInner(
      const rtcp::TransportFeedback& feedback,
      Timestamp feedback_time) RTC_RUN_ON(&lock_);

  void SignalObservers(
      const std::vector<PacketFeedback>& packet_feedback_vector);

  rtc::CriticalSection lock_;
  DataSize pending_untracked_size_ RTC_GUARDED_BY(&lock_) = DataSize::Zero();
  Timestamp last_send_time_ RTC_GUARDED_BY(&lock_) = Timestamp::MinusInfinity();
  Timestamp last_untracked_send_time_ RTC_GUARDED_BY(&lock_) =
      Timestamp::MinusInfinity();
  SequenceNumberUnwrapper seq_num_unwrapper_ RTC_GUARDED_BY(&lock_);
  std::map<int64_t, PacketFeedback> history_ RTC_GUARDED_BY(&lock_);

  // Sequence numbers are never negative, using -1 as it always < a real
  // sequence number.
  int64_t last_ack_seq_num_ RTC_GUARDED_BY(&lock_) = -1;
  InFlightBytesTracker in_flight_ RTC_GUARDED_BY(&lock_);

  Timestamp current_offset_ RTC_GUARDED_BY(&lock_) = Timestamp::MinusInfinity();
  TimeDelta last_timestamp_ RTC_GUARDED_BY(&lock_) = TimeDelta::MinusInfinity();

  uint16_t local_net_id_ RTC_GUARDED_BY(&lock_) = 0;
  uint16_t remote_net_id_ RTC_GUARDED_BY(&lock_) = 0;

  rtc::CriticalSection observers_lock_;
  // Maps a set of ssrcs to corresponding observer. Vectors are used rather than
  // set/map to ensure that the processing order is consistent independently of
  // the randomized ssrcs.
  std::vector<std::pair<std::vector<uint32_t>, StreamFeedbackObserver*>>
      observers_ RTC_GUARDED_BY(&observers_lock_);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_TRANSPORT_FEEDBACK_ADAPTER_H_
