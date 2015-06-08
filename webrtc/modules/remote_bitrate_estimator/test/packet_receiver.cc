/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"

#include <math.h>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace testing {
namespace bwe {

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe)
    : PacketProcessor(listener, flow_id, kReceiver),
      delay_log_prefix_(),
      metric_log_prefix_(),
      packet_loss_log_prefix_(),
      last_delay_plot_ms_(0),
      last_metric_plot_ms_(0),
      last_packet_loss_plot_ms_(0),
      plot_delay_(plot_delay),
      // TODO(magalhaesc) Add separated plot_objective_function and
      // plot_packet_loss parameters to the constructor.
      plot_objective_function_(plot_delay),
      plot_packet_loss_(plot_delay),
      bwe_receiver_(CreateBweReceiver(bwe_type, flow_id, plot_bwe)),
      total_delay_ms_(0),
      total_throughput_(0),
      number_packets_(0) {
  // Setup the prefix ststd::rings used when logging.
  std::stringstream ss1;
  ss1 << "Delay_" << flow_id << "#2";
  delay_log_prefix_ = ss1.str();

  std::stringstream ss2;
  ss2 << "Objective_function_" << flow_id << "#2";
  metric_log_prefix_ = ss2.str();

  std::stringstream ss3;
  ss3 << "Packet_Loss_" << flow_id << "#2";
  packet_loss_log_prefix_ = ss3.str();
}

PacketReceiver::~PacketReceiver() {
}

void PacketReceiver::RunFor(int64_t time_ms, Packets* in_out) {
  Packets feedback;
  for (auto it = in_out->begin(); it != in_out->end();) {
    // PacketReceivers are only associated with a single stream, and therefore
    // should only process a single flow id.
    // TODO(holmer): Break this out into a Demuxer which implements both
    // PacketProcessorListener and PacketProcessor.
    BWE_TEST_LOGGING_CONTEXT("Receiver");
    if ((*it)->GetPacketType() == Packet::kMedia &&
        (*it)->flow_id() == *flow_ids().begin()) {
      BWE_TEST_LOGGING_CONTEXT(*flow_ids().begin());
      const MediaPacket* media_packet = static_cast<const MediaPacket*>(*it);
      // We're treating the send time (from previous filter) as the arrival
      // time once packet reaches the estimator.
      int64_t arrival_time_ms = (media_packet->send_time_us() + 500) / 1000;
      int64_t send_time_ms = (media_packet->creation_time_us() + 500) / 1000;
      delay_stats_.Push(arrival_time_ms - send_time_ms);
      PlotDelay(arrival_time_ms, send_time_ms);
      PlotObjectiveFunction(arrival_time_ms);
      PlotPacketLoss(arrival_time_ms);

      total_delay_ms_ += arrival_time_ms - send_time_ms;
      total_throughput_ += media_packet->payload_size();
      ++number_packets_;

      bwe_receiver_->ReceivePacket(arrival_time_ms, *media_packet);
      FeedbackPacket* fb = bwe_receiver_->GetFeedback(arrival_time_ms);
      if (fb)
        feedback.push_back(fb);
      delete media_packet;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  // Insert feedback packets to be sent back to the sender.
  in_out->merge(feedback, DereferencingComparator<Packet>);
}

void PacketReceiver::PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms) {
  static const int kDelayPlotIntervalMs = 100;
  if (!plot_delay_)
    return;
  if (arrival_time_ms - last_delay_plot_ms_ > kDelayPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT(0, delay_log_prefix_, arrival_time_ms,
                          arrival_time_ms - send_time_ms);
    last_delay_plot_ms_ = arrival_time_ms;
  }
}

double PacketReceiver::ObjectiveFunction() {
  const double kDelta = 1.0;  // Delay penalty factor.
  double throughput_metric = log(static_cast<double>(total_throughput_));
  double delay_penalty = kDelta * log(static_cast<double>(total_delay_ms_));
  return throughput_metric - delay_penalty;
}

void PacketReceiver::PlotObjectiveFunction(int64_t arrival_time_ms) {
  static const int kMetricPlotIntervalMs = 1000;
  if (!plot_objective_function_) {
    return;
  }
  if (arrival_time_ms - last_metric_plot_ms_ > kMetricPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT(1, metric_log_prefix_, arrival_time_ms,
                          ObjectiveFunction());
    last_metric_plot_ms_ = arrival_time_ms;
  }
}

void PacketReceiver::PlotPacketLoss(int64_t arrival_time_ms) {
  static const int kPacketLossPlotIntervalMs = 500;
  if (!plot_packet_loss_) {
    return;
  }
  if (arrival_time_ms - last_packet_loss_plot_ms_ > kPacketLossPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT(2, packet_loss_log_prefix_, arrival_time_ms,
                          bwe_receiver_->RecentPacketLossRatio());
    last_packet_loss_plot_ms_ = arrival_time_ms;
  }
}

Stats<double> PacketReceiver::GetDelayStats() const {
  return delay_stats_;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
