/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_OVERUSE_PREDICTOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_OVERUSE_PREDICTOR_H_

#include <deque>
#include <string>

#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"

namespace webrtc {

struct OverusePredictorConfig {
  FieldTrialFlag enabled{"Enabled"};
  FieldTrialParameter<double> capacity_dev_ratio_threshold{"dev_thr", 0.2};
  FieldTrialParameter<double> capacity_deviation{"cap_dev", -1};
  FieldTrialParameter<TimeDelta> delay_threshold{"del_thr", TimeDelta::ms(100)};
  explicit OverusePredictorConfig(const std::string& config);
};

class OverusePredictor {
 public:
  explicit OverusePredictor(const WebRtcKeyValueConfig* config);
  void OnSentPacket(SentPacket sent_packet);
  bool Enabled() const;
  bool PredictOveruse(const NetworkStateEstimate& est);

 private:
  struct SentPacketInfo {
    Timestamp send_time;
    DataSize size;
  };
  TimeDelta PredictDelay(const NetworkStateEstimate& est);
  const OverusePredictorConfig conf_;
  std::deque<SentPacketInfo> pending_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_OVERUSE_PREDICTOR_H_
