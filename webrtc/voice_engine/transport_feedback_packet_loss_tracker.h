/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_
#define WEBRTC_VOICE_ENGINE_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_

#include <map>

#include "webrtc/base/optional.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {

namespace rtcp {
class TransportFeedback;
}

class TransportFeedbackPacketLossTracker final {
 public:
  // Up to |max_window_size| latest packet statuses wil be used for calculating
  // the packet loss metrics. When less than |min_window_size| samples are
  // available for making a reliable estimation, GetPacketLossRates() will
  // return false to indicate packet loss metrics are not ready.
  TransportFeedbackPacketLossTracker(size_t min_window_size,
                                     size_t max_window_size);

  void OnReceivedTransportFeedback(const rtcp::TransportFeedback& feedback);

  // Returns true if packet loss rate and packet loss episode duration are ready
  // and assigns respective values to |*packet_loss_rate| and
  // |*consecutive_packet_loss_rate|. Continuous packet loss rate is defined as
  // the probability of losing two adjacent packets.
  bool GetPacketLossRates(float* packet_loss_rate,
                          float* consecutive_packet_loss_rate) const;

  // Verifies that the internal states are correct. Only used for tests.
  void Validate() const;

 private:
  // PacketStatus is a map from sequence number to its reception status. The
  // status is true if the corresponding packet is received, and false if it is
  // lost. Unknown statuses are not present in the map.
  typedef std::map<uint16_t, bool> PacketStatus;
  typedef PacketStatus::const_iterator PacketStatusIterator;

  void Reset();
  // ReferenceSequenceNumber() provides a sequence number that defines the
  // order of packet reception info stored in |packet_status_window_|. In
  // particular, given any sequence number |x|,
  // (2^16 + x - ref_seq_num_) % 2^16 defines its actual position in
  // |packet_status_window_|.
  uint16_t ReferenceSequenceNumber() const;
  bool IsOldSequenceNumber(uint16_t seq_num) const;
  void InsertPacketStatus(uint16_t seq_num, bool received);
  void RemoveOldestPacketStatus();
  void ApplyPacketStatus(PacketStatusIterator it);
  void UndoPacketStatus(PacketStatusIterator it);
  PacketStatusIterator PreviousPacketStatus(PacketStatusIterator it);
  PacketStatusIterator NextPacketStatus(PacketStatusIterator it);

  const size_t min_window_size_;
  const size_t max_window_size_;

  PacketStatus packet_status_window_;
  // |ref_packet_status_| points to the oldest item in |packet_status_window_|.
  PacketStatusIterator ref_packet_status_;

  size_t num_received_packets_;
  size_t num_lost_packets_;
  size_t num_consecutive_losses_;
  size_t num_consecutive_old_reports_;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_
