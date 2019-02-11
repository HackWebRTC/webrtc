/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
#define TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_

#include <memory>

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/random.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network/traffic_route.h"

namespace webrtc {
namespace test {

struct RandomWalkConfig {
  int random_seed = 1;
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  TimeDelta update_interval = TimeDelta::ms(200);
  double variance = 0.6;
  double bias = -0.1;
};

class RandomWalkCrossTraffic {
 public:
  RandomWalkCrossTraffic(RandomWalkConfig config, TrafficRoute* traffic_route);
  ~RandomWalkCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  RandomWalkConfig config_;
  TrafficRoute* const traffic_route_;
  webrtc::Random random_;

  Timestamp last_process_time_ = Timestamp::MinusInfinity();
  Timestamp last_update_time_ = Timestamp::MinusInfinity();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  double intensity_ = 0;
  DataSize pending_size_ = DataSize::Zero();
};

struct PulsedPeaksConfig {
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  TimeDelta send_duration = TimeDelta::ms(100);
  TimeDelta hold_duration = TimeDelta::ms(2000);
};

class PulsedPeaksCrossTraffic {
 public:
  PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                          TrafficRoute* traffic_route);
  ~PulsedPeaksCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  PulsedPeaksConfig config_;
  TrafficRoute* const traffic_route_;

  Timestamp last_update_time_ = Timestamp::MinusInfinity();
  Timestamp last_send_time_ = Timestamp::MinusInfinity();
  bool sending_ = false;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_CROSS_TRAFFIC_H_
