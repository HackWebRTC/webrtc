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
                               bool plot_bwe,
                               MetricRecorder* metric_recorder)
    : PacketProcessor(listener, flow_id, kReceiver),
      bwe_receiver_(CreateBweReceiver(bwe_type, flow_id, plot_bwe)),
      metric_recorder_(metric_recorder) {
  if (metric_recorder_ != nullptr) {
    // Setup the prefix ststd::rings used when logging.
    std::vector<std::string> prefixes;

    std::stringstream ss1;
    ss1 << "Throughput_kbps_" << flow_id << "#2";
    prefixes.push_back(ss1.str());  // Throughput.

    std::stringstream ss2;
    ss2 << "Delay_ms_" << flow_id << "#2";
    prefixes.push_back(ss2.str());  // Delay.

    std::stringstream ss3;
    ss3 << "Packet_Loss_" << flow_id << "#2";
    prefixes.push_back(ss3.str());  // Loss.

    std::stringstream ss4;
    ss4 << "Objective_function_" << flow_id << "#2";
    prefixes.push_back(ss4.str());  // Objective.

    // Plot Total/PerFlow Available capacity together with throughputs.
    std::stringstream ss5;
    ss5 << "Throughput_kbps" << flow_id << "#1";
    prefixes.push_back(ss5.str());  // Total Available.
    prefixes.push_back(ss5.str());  // Available per flow.

    metric_recorder_->SetPlotInformation(prefixes);
  }
}

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe)
    : PacketReceiver(listener,
                     flow_id,
                     bwe_type,
                     plot_delay,
                     plot_bwe,
                     nullptr) {
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
      int64_t arrival_time_ms = media_packet->send_time_ms();
      int64_t send_time_ms = media_packet->creation_time_ms();
      delay_stats_.Push(arrival_time_ms - send_time_ms);

      if (metric_recorder_ != nullptr) {
        metric_recorder_->UpdateTime(arrival_time_ms);
        UpdateMetrics(arrival_time_ms, send_time_ms,
                      media_packet->payload_size());
        metric_recorder_->PlotAllDynamics();
      }

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

void PacketReceiver::UpdateMetrics(int64_t arrival_time_ms,
                                   int64_t send_time_ms,
                                   size_t payload_size) {
  metric_recorder_->UpdateThroughput(bwe_receiver_->RecentKbps(), payload_size);
  metric_recorder_->UpdateDelay(arrival_time_ms - send_time_ms);
  metric_recorder_->UpdateLoss(bwe_receiver_->RecentPacketLossRatio());
  metric_recorder_->UpdateObjective();
}

float PacketReceiver::GlobalPacketLoss() {
  return bwe_receiver_->GlobalReceiverPacketLossRatio();
}

Stats<double> PacketReceiver::GetDelayStats() const {
  return delay_stats_;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
