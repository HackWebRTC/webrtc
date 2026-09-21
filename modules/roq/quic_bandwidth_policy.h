/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_
#define MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

struct QuicBandwidthSample {
  Timestamp at_time = Timestamp::MinusInfinity();
  std::vector<uint8_t> connection_id;
  uint64_t acked_bytes = 0;
  bool startup_complete = false;
  DataRate max_bandwidth = DataRate::Zero();
  DataRate delivery_rate = DataRate::Zero();
  TimeDelta round_trip_time = TimeDelta::Zero();
  DataRate audio_payload_rate = DataRate::Zero();
  DataRate maximum_media_rate = DataRate::Zero();
  // Sum of native per-video resume thresholds plus audio; zero without video.
  DataRate resume_media_rate = DataRate::Zero();
  uint64_t evidence_round = 0;
  DataRate evidence_rate = DataRate::Zero();
  TimeDelta evidence_age = TimeDelta::PlusInfinity();
  uint64_t audio_payload_bytes = 0;
  uint64_t audio_wire_bytes = 0;
  uint64_t audio_packets = 0;
  uint64_t audio_redundant_payload_bytes = 0;
  uint64_t audio_redundant_blocks = 0;
  uint64_t audio_packet_rate = 50;  // 20-ms packetization, not DTX cadence.
  uint64_t video_payload_bytes = 0;
  uint64_t video_wire_bytes = 0;
  uint64_t rtcp_wire_bytes = 0;
  TimeDelta oldest_video_queue_age = TimeDelta::Zero();
  uint64_t expired_video_frames = 0;
};

struct QuicMediaBudget {
  bool fresh = false;
  bool capacity_known = false;
  const char* capacity_state = "discovering";
  double audio_redundancy_multiplier = 2.0;
  DataRate audio_header_rate = DataRate::Zero();
  DataRate raw_capacity = DataRate::Zero();
  DataRate overhead = DataRate::Zero();
  DataRate recovery_reserve = DataRate::Zero();
  DataRate safety_headroom = DataRate::Zero();
  DataRate probe_max_rate = DataRate::Zero();
  DataRate maintenance_rate = DataRate::Zero();
  bool resume_fast_path = false;
  DataRate usable_media_rate = DataRate::Zero();
  DataRate target_rate = DataRate::KilobitsPerSec(1000);
  TimeDelta round_trip_time = TimeDelta::Zero();
};

class QuicBandwidthPolicy {
 public:
  QuicBandwidthPolicy(DataRate minimum_rate = DataRate::KilobitsPerSec(256),
                      DataRate startup_rate = DataRate::KilobitsPerSec(1000),
                      DataRate maximum_rate = DataRate::PlusInfinity());
  void SetLimits(DataRate minimum_rate,
                 DataRate startup_rate,
                 DataRate maximum_rate);
  QuicMediaBudget Update(const QuicBandwidthSample& sample);

 private:
  std::vector<uint8_t> connection_id_;
  std::optional<Timestamp> demand_started_at_;
  std::optional<Timestamp> demand_satisfied_at_;
  const char* capacity_state_ = "discovering";
  bool capacity_known_ = false;
  std::optional<QuicBandwidthSample> cost_sample_;
  // One RED copy initially. Headers scale with packets, not codec bytes.
  double audio_redundancy_multiplier_ = 2.0;
  double audio_header_bytes_per_packet_ = 118;
  double audio_packet_rate_ = 50;
  double video_wire_ratio_ = 1.08;  // Replaced by measured RTP/QUIC cost.
  DataRate rtcp_rate_ = DataRate::KilobitsPerSec(4);
  uint64_t last_acked_bytes_ = 0;
  std::optional<Timestamp> last_ack_time_;
  std::optional<uint64_t> resume_after_round_;
  DataRate last_resume_demand_ = DataRate::Zero();
  uint64_t last_evidence_round_ = 0;
  DataRate minimum_rate_;
  DataRate startup_rate_;
  DataRate maximum_rate_;
  DataRate target_rate_;
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_
