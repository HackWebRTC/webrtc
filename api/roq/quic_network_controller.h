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
  DataRate raw_capacity = DataRate::Zero();
  DataRate overhead = DataRate::Zero();
  DataRate recovery_reserve = DataRate::Zero();
  DataRate safety_headroom = DataRate::Zero();
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

  explicit QuicNetworkControllerFactory(StatsProvider stats_provider,
                                        UpdateObserver update_observer = {});

  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;

 private:
  const StatsProvider stats_provider_;
  const UpdateObserver update_observer_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_QUIC_NETWORK_CONTROLLER_H_
