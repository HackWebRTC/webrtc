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
  // * Up to |max_window_size| latest packet statuses will be used for
  //   calculating the packet loss metrics.
  // * PLR (packet-loss-rate) is reliably computable once the statuses of
  //   |plr_min_num_packets| packets are known.
  // * RPLR (recoverable-packet-loss-rate) is reliably computable once the
  //   statuses of |rplr_min_num_pairs| pairs are known.
  TransportFeedbackPacketLossTracker(size_t max_window_size,
                                     size_t plr_min_num_packets,
                                     size_t rplr_min_num_pairs);

  void OnReceivedTransportFeedback(const rtcp::TransportFeedback& feedback);

  // Returns the packet loss rate, if the window has enough packet statuses to
  // reliably compute it. Otherwise, returns empty.
  rtc::Optional<float> GetPacketLossRate() const;

  // Returns the first-order-FEC recoverable packet loss rate, if the window has
  // enough status pairs to reliably compute it. Otherwise, returns empty.
  rtc::Optional<float> GetRecoverablePacketLossRate() const;

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

  void UpdateMetrics(PacketStatusIterator it, bool apply /* false = undo */);
  void UpdatePlr(PacketStatusIterator it, bool apply /* false = undo */);
  void UpdateRplr(PacketStatusIterator it, bool apply /* false = undo */);

  PacketStatusIterator PreviousPacketStatus(PacketStatusIterator it);
  PacketStatusIterator NextPacketStatus(PacketStatusIterator it);

  const size_t max_window_size_;

  PacketStatus packet_status_window_;
  // |ref_packet_status_| points to the oldest item in |packet_status_window_|.
  PacketStatusIterator ref_packet_status_;

  // Packet-loss-rate calculation (lost / all-known-packets).
  struct PlrState {
    explicit PlrState(size_t min_num_packets)
        : min_num_packets_(min_num_packets) {
      Reset();
    }
    void Reset() {
      num_received_packets_ = 0;
      num_lost_packets_ = 0;
    }
    rtc::Optional<float> GetMetric() const;
    const size_t min_num_packets_;
    size_t num_received_packets_;
    size_t num_lost_packets_;
  } plr_state_;

  // Recoverable packet loss calculation (first-order-FEC recoverable).
  struct RplrState {
    explicit RplrState(size_t min_num_pairs)
        : min_num_pairs_(min_num_pairs) {
      Reset();
    }
    void Reset() {
      num_known_pairs_ = 0;
      num_recoverable_losses_ = 0;
    }
    rtc::Optional<float> GetMetric() const;
    // Recoverable packets are those which were lost, but immediately followed
    // by a properly received packet. If that second packet carried FEC,
    // the data from the former (lost) packet could be recovered.
    // The RPLR is calculated as the fraction of such pairs (lost-received) out
    // of all pairs of consecutive acked packets.
    const size_t min_num_pairs_;
    size_t num_known_pairs_;
    size_t num_recoverable_losses_;
  } rplr_state_;

  size_t num_consecutive_old_reports_;  // TODO(elad.alon): Upcoming CL removes.
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_
