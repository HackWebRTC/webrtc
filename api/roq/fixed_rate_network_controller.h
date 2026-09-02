/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_FIXED_RATE_NETWORK_CONTROLLER_H_
#define API_ROQ_FIXED_RATE_NETWORK_CONTROLLER_H_

#include <memory>

#include "api/transport/network_control.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"

namespace webrtc::roq {

class FixedRateNetworkControllerFactory final
    : public NetworkControllerFactoryInterface {
 public:
  explicit FixedRateNetworkControllerFactory(DataRate media_rate);

  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;

 private:
  const DataRate media_rate_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_FIXED_RATE_NETWORK_CONTROLLER_H_
