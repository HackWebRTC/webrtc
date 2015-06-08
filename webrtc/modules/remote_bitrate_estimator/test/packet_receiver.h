/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

namespace webrtc {
namespace testing {
namespace bwe {

class PacketReceiver : public PacketProcessor {
 public:
  PacketReceiver(PacketProcessorListener* listener,
                 int flow_id,
                 BandwidthEstimatorType bwe_type,
                 bool plot_delay,
                 bool plot_bwe);
  ~PacketReceiver();

  // Implements PacketProcessor.
  void RunFor(int64_t time_ms, Packets* in_out) override;

  void LogStats();

  Stats<double> GetDelayStats() const;

 protected:
  void PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms);
  void PlotObjectiveFunction(int64_t arrival_time_ms);
  void PlotPacketLoss(int64_t arrival_time_ms);
  double ObjectiveFunction();

  int64_t now_ms_;
  std::string delay_log_prefix_;
  std::string metric_log_prefix_;
  std::string packet_loss_log_prefix_;
  int64_t last_delay_plot_ms_;
  int64_t last_metric_plot_ms_;
  int64_t last_packet_loss_plot_ms_;
  bool plot_delay_;
  bool plot_objective_function_;
  bool plot_packet_loss_;
  Stats<double> delay_stats_;
  rtc::scoped_ptr<BweReceiver> bwe_receiver_;

  int64_t total_delay_ms_;
  size_t total_throughput_;
  int number_packets_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PacketReceiver);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_
