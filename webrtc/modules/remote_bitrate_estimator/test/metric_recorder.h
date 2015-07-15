/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_METRIC_RECORDER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_METRIC_RECORDER_H_

#include <set>
#include <string>
#include <vector>

#include "webrtc/base/common.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"

namespace webrtc {
namespace testing {
namespace bwe {

class LinkShare {
 public:
  explicit LinkShare(ChokeFilter* choke_filter);

  void PauseFlow(int flow_id);   // Increases available capacity per flow.
  void ResumeFlow(int flow_id);  // Decreases available capacity per flow.

  uint32_t TotalAvailableKbps();
  // If the given flow is paused, its output is zero.
  uint32_t AvailablePerFlowKbps(int flow_id);

 private:
  ChokeFilter* choke_filter_;
  std::set<int> running_flows_;
};

struct PlotInformation {
  PlotInformation()
      : prefix(),
        last_plot_ms(0),
        time_ms(0),
        value(0.0),
        plot_interval_ms(0) {}
  template <typename T>
  void Update(int64_t now_ms, T new_value) {
    time_ms = now_ms;
    value = static_cast<double>(new_value);
  }
  std::string prefix;
  bool plot;
  int64_t last_plot_ms;
  int64_t time_ms;
  double value;
  int64_t plot_interval_ms;
};

class MetricRecorder {
 public:
  MetricRecorder(const std::string algorithm_name,
                 int flow_id,
                 PacketSender* packet_sender,
                 LinkShare* link_share);

  void SetPlotInformation(const std::vector<std::string>& prefixes);

  template <typename T>
  void PlotLine(int windows_id,
                const std::string& prefix,
                int64_t time_ms,
                T y);

  void PlotDynamics(int metric);
  void PlotAllDynamics();

  void UpdateTime(int64_t time_ms);
  void UpdateThroughput(int64_t bitrate_kbps, size_t payload_size);
  void UpdateDelay(int64_t delay_ms);
  void UpdateLoss(float loss_ratio);
  void UpdateObjective();

  void PlotThroughputHistogram(const std::string& title,
                               const std::string& bwe_name,
                               int num_flows,
                               int64_t extra_offset_ms,
                               const std::string optimum_id);

  void PlotThroughputHistogram(const std::string& title,
                               const std::string& bwe_name,
                               int num_flows,
                               int64_t extra_offset_ms);

  void PlotDelayHistogram(const std::string& title,
                          const std::string& bwe_name,
                          int num_flows,
                          int64_t one_way_path_delay_ms);

  void PlotLossHistogram(const std::string& title,
                         const std::string& bwe_name,
                         int num_flows,
                         float global_loss_ratio);

  void PlotObjectiveHistogram(const std::string& title,
                              const std::string& bwe_name,
                              int num_flows);

  void set_start_computing_metrics_ms(int64_t start_computing_metrics_ms) {
    start_computing_metrics_ms_ = start_computing_metrics_ms;
  }

  void set_plot_available_capacity(bool plot) {
    plot_information_[kTotalAvailable].plot = plot;
  }

  void PauseFlow();                         // Plot zero.
  void ResumeFlow(int64_t paused_time_ms);  // Plot zero.
  void PlotZero();

 private:
  uint32_t GetTotalAvailableKbps();
  uint32_t GetAvailablePerFlowKbps();
  uint32_t GetSendingEstimateKbps();
  double ObjectiveFunction();

  double Renormalize(double x);
  bool ShouldRecord(int64_t arrival_time_ms);

  void PushDelayMs(int64_t delay_ms, int64_t arrival_time_ms);
  void PushThroughputBytes(size_t throughput_bytes, int64_t arrival_time_ms);

  enum Metrics {
    kThroughput = 0,
    kDelay,
    kLoss,
    kObjective,
    kTotalAvailable,
    kAvailablePerFlow,
    kNumMetrics
  };

  std::string algorithm_name_;
  int flow_id_;
  PacketSender* packet_sender_;
  LinkShare* link_share_;

  int64_t now_ms_;

  PlotInformation plot_information_[kNumMetrics];

  std::vector<int64_t> delays_ms_;
  std::vector<size_t> throughput_bytes_;
  // (Receiving rate - available bitrate per flow) * time window.
  std::vector<int64_t> weighted_estimate_error_;
  int64_t last_unweighted_estimate_error_;
  int64_t optimal_throughput_bits_;
  int64_t last_available_bitrate_per_flow_kbps_;
  int64_t start_computing_metrics_ms_;
  bool started_computing_metrics_;
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_METRIC_RECORDER_H_
