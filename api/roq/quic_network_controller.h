/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_QUIC_NETWORK_CONTROLLER_H_
#define API_ROQ_QUIC_NETWORK_CONTROLLER_H_

#include <functional>
#include <memory>

#include "api/roq/roq_session.h"
#include "api/transport/network_control.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"

namespace webrtc::roq {

struct QuicNetworkControlState {
  Timestamp at_time = Timestamp::MinusInfinity();
  uint64_t acked_bytes = 0;
  bool startup_complete = false;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  TimeDelta round_trip_time = TimeDelta::Zero();
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
  DataRate pacer_rate = DataRate::KilobitsPerSec(1000);
  DataRate padding_rate = DataRate::Zero();
  size_t probe_cluster_count = 0;
  bool has_congestion_window = false;
};

class QuicNetworkControllerFactory final
    : public NetworkControllerFactoryInterface {
 public:
  using StatsProvider = std::function<RoqSessionStats()>;
  using UpdateObserver = std::function<void(const QuicNetworkControlState&)>;

  explicit QuicNetworkControllerFactory(
      StatsProvider stats_provider,
      UpdateObserver update_observer = {},
      DataRate minimum_rate = DataRate::KilobitsPerSec(256),
      DataRate startup_rate = DataRate::KilobitsPerSec(1000),
      DataRate maximum_rate = DataRate::PlusInfinity());

  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;

 private:
  const StatsProvider stats_provider_;
  const UpdateObserver update_observer_;
  const DataRate minimum_rate_;
  const DataRate startup_rate_;
  const DataRate maximum_rate_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_QUIC_NETWORK_CONTROLLER_H_
