/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
#include <stdint.h>
#include <ostream>
#include <vector>
#include "modules/congestion_controller/network_control/include/network_units.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Configuration

// Use StreamsConfig for information about streams that is required for specific
// adjustments to the algorithms in network controllers. Especially useful
// for experiments.
struct StreamsConfig {
  StreamsConfig();
  StreamsConfig(const StreamsConfig&);
  ~StreamsConfig();
  Timestamp at_time;
  bool requests_alr_probing = false;
  rtc::Optional<double> pacing_factor;
  rtc::Optional<DataRate> min_pacing_rate;
  rtc::Optional<DataRate> max_padding_rate;
};

struct TargetRateConstraints {
  Timestamp at_time;
  DataRate starting_rate;
  DataRate min_data_rate;
  DataRate max_data_rate;
};

// Send side information

struct NetworkAvailability {
  Timestamp at_time;
  bool network_available = false;
};

struct NetworkRouteChange {
  Timestamp at_time;
  // The TargetRateConstraints are set here so they can be changed synchronously
  // when network route changes.
  TargetRateConstraints constraints;
};

struct SentPacket {
  Timestamp send_time;
  DataSize size;
  PacedPacketInfo pacing_info;
};

struct PacerQueueUpdate {
  TimeDelta expected_queue_time;
};

// Transport level feedback

struct RemoteBitrateReport {
  Timestamp receive_time;
  DataRate bandwidth;
};

struct RoundTripTimeUpdate {
  Timestamp receive_time;
  TimeDelta round_trip_time;
  bool smoothed = false;
};

struct TransportLossReport {
  Timestamp receive_time;
  Timestamp start_time;
  Timestamp end_time;
  uint64_t packets_lost_delta = 0;
  uint64_t packets_received_delta = 0;
};

struct OutstandingData {
  DataSize in_flight_data;
};

// Packet level feedback

struct PacketResult {
  PacketResult();
  PacketResult(const PacketResult&);
  ~PacketResult();

  rtc::Optional<SentPacket> sent_packet;
  Timestamp receive_time;
};

struct TransportPacketsFeedback {
  TransportPacketsFeedback();
  TransportPacketsFeedback(const TransportPacketsFeedback& other);
  ~TransportPacketsFeedback();

  Timestamp feedback_time;
  DataSize data_in_flight;
  DataSize prior_in_flight;
  std::vector<PacketResult> packet_feedbacks;

  std::vector<PacketResult> ReceivedWithSendInfo() const;
  std::vector<PacketResult> LostWithSendInfo() const;
  std::vector<PacketResult> PacketsWithFeedback() const;
};

// Network estimation

struct NetworkEstimate {
  Timestamp at_time;
  DataRate bandwidth;
  TimeDelta round_trip_time;
  TimeDelta bwe_period;

  float loss_rate_ratio = 0;
  bool changed = true;
};

// Network control
struct CongestionWindow {
  bool enabled = true;
  DataSize data_window;
};

struct PacerConfig {
  Timestamp at_time;
  // Pacer should send at most data_window data over time_window duration.
  DataSize data_window;
  TimeDelta time_window;
  // Pacer should send at least pad_window data over time_window duration.
  DataSize pad_window;
  DataRate data_rate() const { return data_window / time_window; }
};

struct ProbeClusterConfig {
  Timestamp at_time;
  DataRate target_data_rate;
  TimeDelta target_duration;
  uint32_t target_probe_count;
};

struct TargetTransferRate {
  Timestamp at_time;
  DataRate target_rate;
  // The estimate on which the target rate is based on.
  NetworkEstimate network_estimate;
};

// Process control
struct ProcessInterval {
  Timestamp at_time;
};

::std::ostream& operator<<(::std::ostream& os,
                           const ProbeClusterConfig& config);
::std::ostream& operator<<(::std::ostream& os, const PacerConfig& config);
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_INCLUDE_NETWORK_TYPES_H_
