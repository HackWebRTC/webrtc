/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_
#define MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_

#include <cstdint>
#include <optional>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc::roq {

struct QuicBandwidthSample {
  Timestamp at_time = Timestamp::MinusInfinity();
  uint64_t acked_bytes = 0;
  bool startup_complete = false;
  DataRate max_bandwidth = DataRate::Zero();
  DataRate delivery_rate = DataRate::Zero();
  TimeDelta round_trip_time = TimeDelta::Zero();
};

struct QuicMediaBudget {
  bool fresh = false;
  DataRate raw_capacity = DataRate::Zero();
  DataRate overhead = DataRate::Zero();
  DataRate recovery_reserve = DataRate::Zero();
  DataRate safety_headroom = DataRate::Zero();
  DataRate usable_media_rate = DataRate::Zero();
  DataRate target_rate = DataRate::KilobitsPerSec(1000);
  TimeDelta round_trip_time = TimeDelta::Zero();
};

class QuicBandwidthPolicy {
 public:
  QuicMediaBudget Update(const QuicBandwidthSample& sample);

 private:
  uint64_t last_acked_bytes_ = 0;
  std::optional<Timestamp> last_ack_time_;
  DataRate target_rate_ = DataRate::KilobitsPerSec(1000);
};

}  // namespace webrtc::roq

#endif  // MODULES_ROQ_QUIC_BANDWIDTH_POLICY_H_
