/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/metric_recorder.h"

#include <algorithm>

namespace webrtc {
namespace testing {
namespace bwe {

namespace {

template <typename T>
T Sum(const std::vector<T>& input) {
  T total = 0;
  for (T val : input) {
    total += val;
  }
  return total;
}

template <typename T>
double Average(const std::vector<T>& array, size_t size) {
  return static_cast<double>(Sum(array)) / size;
}

template <typename T>
std::vector<T> Abs(const std::vector<T>& input) {
  std::vector<T> output(input);
  for (T val : output) {
    val = std::abs(val);
  }
  return output;
}

template <typename T>
std::vector<double> Pow(const std::vector<T>& input, double p) {
  std::vector<double> output;
  for (T val : input) {
    output.push_back(pow(static_cast<double>(val), p));
  }
  return output;
}

template <typename T>
double StandardDeviation(const std::vector<T>& array, size_t size) {
  double mean = Average(array, size);
  std::vector<double> square_values = Pow(array, 2.0);
  double var = Average(square_values, size) - mean * mean;
  return sqrt(var);
}

// Holder mean, Manhattan distance for p=1, EuclidianNorm/sqrt(n) for p=2.
template <typename T>
double NormLp(const std::vector<T>& array, size_t size, double p) {
  std::vector<T> abs_values = Abs(array);
  std::vector<double> pow_values = Pow(abs_values, p);
  return pow(Sum(pow_values) / size, 1.0 / p);
}

template <typename T>
std::vector<T> PositiveFilter(const std::vector<T>& input) {
  std::vector<T> output(input);
  for (T val : output) {
    val = val > 0 ? val : 0;
  }
  return output;
}

template <typename T>
std::vector<T> NegativeFilter(const std::vector<T>& input) {
  std::vector<T> output(input);
  for (T val : output) {
    val = val < 0 ? -val : 0;
  }
  return output;
}
}  // namespace

LinkShare::LinkShare(ChokeFilter* choke_filter)
    : choke_filter_(choke_filter), running_flows_(choke_filter->flow_ids()) {
}

void LinkShare::PauseFlow(int flow_id) {
  running_flows_.erase(flow_id);
}

void LinkShare::ResumeFlow(int flow_id) {
  running_flows_.insert(flow_id);
}

uint32_t LinkShare::TotalAvailableKbps() {
  return choke_filter_->capacity_kbps();
}

uint32_t LinkShare::AvailablePerFlowKbps(int flow_id) {
  uint32_t available_capacity_per_flow_kbps = 0;
  if (running_flows_.find(flow_id) != running_flows_.end()) {
    available_capacity_per_flow_kbps =
        TotalAvailableKbps() / static_cast<uint32_t>(running_flows_.size());
  }
  return available_capacity_per_flow_kbps;
}

MetricRecorder::MetricRecorder(const std::string algorithm_name,
                               int flow_id,
                               PacketSender* packet_sender,
                               LinkShare* link_share)
    : algorithm_name_(algorithm_name),
      flow_id_(flow_id),
      packet_sender_(packet_sender),
      link_share_(link_share),
      now_ms_(0),
      delays_ms_(),
      throughput_bytes_(),
      weighted_estimate_error_(),
      last_unweighted_estimate_error_(0),
      optimal_throughput_bits_(0),
      last_available_bitrate_per_flow_kbps_(0),
      start_computing_metrics_ms_(0),
      started_computing_metrics_(false) {
}

void MetricRecorder::SetPlotInformation(
    const std::vector<std::string>& prefixes) {
  assert(prefixes.size() == kNumMetrics);
  for (size_t i = 0; i < kNumMetrics; ++i) {
    plot_information_[i].prefix = prefixes[i];
  }
  plot_information_[kThroughput].plot_interval_ms = 100;
  plot_information_[kDelay].plot_interval_ms = 100;
  plot_information_[kLoss].plot_interval_ms = 500;
  plot_information_[kObjective].plot_interval_ms = 1000;
  plot_information_[kTotalAvailable].plot_interval_ms = 1000;
  plot_information_[kAvailablePerFlow].plot_interval_ms = 1000;

  for (int i = kThroughput; i < kNumMetrics; ++i) {
    plot_information_[i].last_plot_ms = 0;
    if (i == kObjective || i == kAvailablePerFlow) {
      plot_information_[i].plot = false;
    } else {
      plot_information_[i].plot = true;
    }
  }
}

void MetricRecorder::PlotAllDynamics() {
  for (int i = kThroughput; i < kNumMetrics; ++i) {
    if (plot_information_[i].plot &&
        now_ms_ - plot_information_[i].last_plot_ms >=
            plot_information_[i].plot_interval_ms) {
      PlotDynamics(i);
    }
  }
}

void MetricRecorder::PlotDynamics(int metric) {
  if (metric == kTotalAvailable) {
    BWE_TEST_LOGGING_PLOT_WITH_NAME(
        0, plot_information_[kTotalAvailable].prefix, now_ms_,
        GetTotalAvailableKbps(), "Available");
  } else if (metric == kAvailablePerFlow) {
    BWE_TEST_LOGGING_PLOT_WITH_NAME(
        0, plot_information_[kAvailablePerFlow].prefix, now_ms_,
        GetAvailablePerFlowKbps(), "Available_per_flow");
  } else {
    PlotLine(metric, plot_information_[metric].prefix,
             plot_information_[metric].time_ms,
             plot_information_[metric].value);
  }
  plot_information_[metric].last_plot_ms = now_ms_;
}

template <typename T>
void MetricRecorder::PlotLine(int windows_id,
                              const std::string& prefix,
                              int64_t time_ms,
                              T y) {
  BWE_TEST_LOGGING_PLOT_WITH_NAME(windows_id, prefix, time_ms,
                                  static_cast<double>(y), algorithm_name_);
}

void MetricRecorder::UpdateTime(int64_t time_ms) {
  now_ms_ = std::max(now_ms_, time_ms);
}

void MetricRecorder::UpdateThroughput(int64_t bitrate_kbps,
                                      size_t payload_size) {
  // Total throughput should be computed before updating the time.
  PushThroughputBytes(payload_size, now_ms_);
  plot_information_[kThroughput].Update(now_ms_, bitrate_kbps);
}

void MetricRecorder::UpdateDelay(int64_t delay_ms) {
  PushDelayMs(delay_ms, now_ms_);
  plot_information_[kDelay].Update(now_ms_, delay_ms);
}

void MetricRecorder::UpdateLoss(float loss_ratio) {
  plot_information_[kLoss].Update(now_ms_, loss_ratio);
}

void MetricRecorder::UpdateObjective() {
  plot_information_[kObjective].Update(now_ms_, ObjectiveFunction());
}

uint32_t MetricRecorder::GetTotalAvailableKbps() {
  return link_share_->TotalAvailableKbps();
}

uint32_t MetricRecorder::GetAvailablePerFlowKbps() {
  return link_share_->AvailablePerFlowKbps(flow_id_);
}

uint32_t MetricRecorder::GetSendingEstimateKbps() {
  return packet_sender_->TargetBitrateKbps();
}

void MetricRecorder::PushDelayMs(int64_t delay_ms, int64_t arrival_time_ms) {
  if (ShouldRecord(arrival_time_ms)) {
    delays_ms_.push_back(delay_ms);
  }
}

void MetricRecorder::PushThroughputBytes(size_t payload_size,
                                         int64_t arrival_time_ms) {
  if (ShouldRecord(arrival_time_ms)) {
    throughput_bytes_.push_back(payload_size);

    int64_t current_available_per_flow_kbps =
        static_cast<int64_t>(GetAvailablePerFlowKbps());

    int64_t current_bitrate_diff_kbps =
        static_cast<int64_t>(GetSendingEstimateKbps()) -
        current_available_per_flow_kbps;

    weighted_estimate_error_.push_back(
        ((current_bitrate_diff_kbps + last_unweighted_estimate_error_) *
         (arrival_time_ms - plot_information_[kThroughput].time_ms)) /
        2);

    optimal_throughput_bits_ +=
        ((current_available_per_flow_kbps +
          last_available_bitrate_per_flow_kbps_) *
         (arrival_time_ms - plot_information_[kThroughput].time_ms)) /
        2;

    last_available_bitrate_per_flow_kbps_ = current_available_per_flow_kbps;
  }
}

bool MetricRecorder::ShouldRecord(int64_t arrival_time_ms) {
  if (arrival_time_ms >= start_computing_metrics_ms_) {
    if (!started_computing_metrics_) {
      start_computing_metrics_ms_ = arrival_time_ms;
      now_ms_ = arrival_time_ms;
      started_computing_metrics_ = true;
    }
    return true;
  } else {
    return false;
  }
}

// The weighted_estimate_error_ was weighted based on time windows.
// This function scales back the result before plotting.
double MetricRecorder::Renormalize(double x) {
  size_t num_packets_received = delays_ms_.size();
  return (x * num_packets_received) / now_ms_;
}

inline double U(int64_t x, double alpha) {
  if (alpha == 1.0) {
    return log(static_cast<double>(x));
  }
  return pow(static_cast<double>(x), 1.0 - alpha) / (1.0 - alpha);
}

inline double U(size_t x, double alpha) {
  return U(static_cast<int64_t>(x), alpha);
}

// TODO(magalhaesc): Update ObjectiveFunction.
double MetricRecorder::ObjectiveFunction() {
  const double kDelta = 0.15;  // Delay penalty factor.
  const double kAlpha = 1.0;
  const double kBeta = 1.0;

  double throughput_metric = U(Sum(throughput_bytes_), kAlpha);
  double delay_penalty = kDelta * U(Sum(delays_ms_), kBeta);

  return throughput_metric - delay_penalty;
}

void MetricRecorder::PlotThroughputHistogram(const std::string& title,
                                             const std::string& bwe_name,
                                             int num_flows,
                                             int64_t extra_offset_ms,
                                             const std::string optimum_id) {
  size_t num_packets_received = delays_ms_.size();

  int64_t duration_ms = now_ms_ - start_computing_metrics_ms_ - extra_offset_ms;

  double average_bitrate_kbps =
      static_cast<double>(8 * Sum(throughput_bytes_) / duration_ms);

  double optimal_bitrate_per_flow_kbps =
      static_cast<double>(optimal_throughput_bits_ / duration_ms);

  std::vector<int64_t> positive = PositiveFilter(weighted_estimate_error_);
  std::vector<int64_t> negative = NegativeFilter(weighted_estimate_error_);

  double p_error = Renormalize(NormLp(positive, num_packets_received, 1.0));
  double n_error = Renormalize(NormLp(negative, num_packets_received, 1.0));

  // Prevent the error to be too close to zero (plotting issue).
  double extra_error = average_bitrate_kbps / 500;

  std::string optimum_title =
      optimum_id.empty() ? "optimal_bitrate" : "optimal_bitrates#" + optimum_id;

  BWE_TEST_LOGGING_LABEL(4, title, "average_bitrate_(kbps)", num_flows);
  BWE_TEST_LOGGING_LIMITERRORBAR(
      4, bwe_name, average_bitrate_kbps,
      average_bitrate_kbps - n_error - extra_error,
      average_bitrate_kbps + p_error + extra_error, "estimate_error",
      optimal_bitrate_per_flow_kbps, optimum_title, flow_id_);

  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Channel utilization : ",
                        "%lf %%",
                        100.0 * static_cast<double>(average_bitrate_kbps) /
                            optimal_bitrate_per_flow_kbps);

  RTC_UNUSED(p_error);
  RTC_UNUSED(n_error);
  RTC_UNUSED(extra_error);
  RTC_UNUSED(optimal_bitrate_per_flow_kbps);
}

void MetricRecorder::PlotThroughputHistogram(const std::string& title,
                                             const std::string& bwe_name,
                                             int num_flows,
                                             int64_t extra_offset_ms) {
  PlotThroughputHistogram(title, bwe_name, num_flows, extra_offset_ms, "");
}

void MetricRecorder::PlotDelayHistogram(const std::string& title,
                                        const std::string& bwe_name,
                                        int num_flows,
                                        int64_t one_way_path_delay_ms) {
  size_t num_packets_received = delays_ms_.size();
  double average_delay_ms = Average(delays_ms_, num_packets_received);

  // Prevent the error to be too close to zero (plotting issue).
  double extra_error = average_delay_ms / 500;

  double tenth_sigma_ms =
      StandardDeviation(delays_ms_, num_packets_received) / 10.0 + extra_error;

  size_t per_5_index = (num_packets_received - 1) / 20;
  std::nth_element(delays_ms_.begin(), delays_ms_.begin() + per_5_index,
                   delays_ms_.end());
  int64_t percentile_5_ms = delays_ms_[per_5_index];

  size_t per_95_index = num_packets_received - 1 - per_5_index;
  std::nth_element(delays_ms_.begin(), delays_ms_.begin() + per_95_index,
                   delays_ms_.end());
  int64_t percentile_95_ms = delays_ms_[per_95_index];

  BWE_TEST_LOGGING_LABEL(5, title, "average_delay_(ms)", num_flows)
  BWE_TEST_LOGGING_ERRORBAR(5, bwe_name, average_delay_ms, percentile_5_ms,
                            percentile_95_ms, "5th and 95th percentiles",
                            flow_id_);

  // Log added latency, disregard baseline path delay.
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay average : ",
                        "%lf ms", average_delay_ms - one_way_path_delay_ms);
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay 5th percentile : ",
                        "%ld ms", percentile_5_ms - one_way_path_delay_ms);
  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Delay 95th percentile : ",
                        "%ld ms", percentile_95_ms - one_way_path_delay_ms);

  RTC_UNUSED(tenth_sigma_ms);
  RTC_UNUSED(percentile_5_ms);
  RTC_UNUSED(percentile_95_ms);
}

void MetricRecorder::PlotLossHistogram(const std::string& title,
                                       const std::string& bwe_name,
                                       int num_flows,
                                       float global_loss_ratio) {
  BWE_TEST_LOGGING_LABEL(6, title, "packet_loss_ratio_(%)", num_flows)
  BWE_TEST_LOGGING_BAR(6, bwe_name, 100.0f * global_loss_ratio, flow_id_);

  BWE_TEST_LOGGING_LOG1("RESULTS >>> " + bwe_name + " Loss Ratio : ", "%f %%",
                        100.0f * global_loss_ratio);
}

void MetricRecorder::PlotObjectiveHistogram(const std::string& title,
                                            const std::string& bwe_name,
                                            int num_flows) {
  BWE_TEST_LOGGING_LABEL(7, title, "objective_function", num_flows)
  BWE_TEST_LOGGING_BAR(7, bwe_name, ObjectiveFunction(), flow_id_);
}

void MetricRecorder::PlotZero() {
  for (int i = kThroughput; i <= kLoss; ++i) {
    if (plot_information_[i].plot) {
      std::stringstream prefix;
      prefix << "Receiver_" << flow_id_ << "_" + plot_information_[i].prefix;
      PlotLine(i, prefix.str(), now_ms_, 0);
      plot_information_[i].last_plot_ms = now_ms_;
    }
  }
}

void MetricRecorder::PauseFlow() {
  PlotZero();
  link_share_->PauseFlow(flow_id_);
}

void MetricRecorder::ResumeFlow(int64_t paused_time_ms) {
  UpdateTime(now_ms_ + paused_time_ms);
  PlotZero();
  link_share_->ResumeFlow(flow_id_);
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
