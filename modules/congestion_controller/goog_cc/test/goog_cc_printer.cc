/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"

#include <math.h>

#include <utility>

#include "absl/types/optional.h"
#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "modules/congestion_controller/goog_cc/trendline_estimator.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
void WriteTypedValue(RtcEventLogOutput* out, int value) {
  LogWriteFormat(out, "%i", value);
}
void WriteTypedValue(RtcEventLogOutput* out, double value) {
  LogWriteFormat(out, "%.6f", value);
}
void WriteTypedValue(RtcEventLogOutput* out, absl::optional<DataRate> value) {
  LogWriteFormat(out, "%.0f", value ? value->bytes_per_sec<double>() : NAN);
}
void WriteTypedValue(RtcEventLogOutput* out, absl::optional<TimeDelta> value) {
  LogWriteFormat(out, "%.3f", value ? value->seconds<double>() : NAN);
}
template <typename F>
class TypedFieldLogger : public FieldLogger {
 public:
  TypedFieldLogger(std::string name, F&& getter)
      : name_(std::move(name)), getter_(std::forward<F>(getter)) {}
  const std::string& name() const override { return name_; }
  void WriteValue(RtcEventLogOutput* out) override {
    WriteTypedValue(out, getter_());
  }

 private:
  std::string name_;
  F getter_;
};

template <typename F>
FieldLogger* Log(std::string name, F&& getter) {
  return new TypedFieldLogger<F>(std::move(name), std::forward<F>(getter));
}

}  // namespace
GoogCcStatePrinter::GoogCcStatePrinter() {
  for (auto* logger : CreateLoggers()) {
    loggers_.emplace_back(logger);
  }
}
const NetworkStateEstimate& GoogCcStatePrinter::GetEst() {
  static NetworkStateEstimate kFallback;
  if (controller_->network_estimator_ &&
      controller_->network_estimator_->GetCurrentEstimate())
    return *controller_->network_estimator_->GetCurrentEstimate();
  return kFallback;
}
std::deque<FieldLogger*> GoogCcStatePrinter::CreateLoggers() {
  auto stable_estimate = [this] {
    return DataRate::kbps(
        controller_->delay_based_bwe_->rate_control_.link_capacity_
            .estimate_kbps_.value_or(-INFINITY));
  };
  auto rate_control_state = [this] {
    return static_cast<int>(
        controller_->delay_based_bwe_->rate_control_.rate_control_state_);
  };
  auto trend = [this] {
    return reinterpret_cast<TrendlineEstimator*>(
        controller_->delay_based_bwe_->delay_detector_.get());
  };
  auto acknowledged_rate = [this] {
    return controller_->acknowledged_bitrate_estimator_->bitrate();
  };
  std::deque<FieldLogger*> loggers({
      Log("rate_control_state", [=] { return rate_control_state(); }),
      Log("stable_estimate", [=] { return stable_estimate(); }),
      Log("trendline", [=] { return trend()->prev_trend_; }),
      Log("trendline_modified_offset",
          [=] { return trend()->prev_modified_trend_; }),
      Log("trendline_offset_threshold", [=] { return trend()->threshold_; }),
      Log("acknowledged_rate", [=] { return acknowledged_rate(); }),
      Log("est_capacity", [=] { return GetEst().link_capacity; }),
      Log("est_capacity_dev", [=] { return GetEst().link_capacity_std_dev; }),
      Log("est_capacity_min", [=] { return GetEst().link_capacity_min; }),
      Log("est_cross_traffic", [=] { return GetEst().cross_traffic_ratio; }),
      Log("est_cross_delay", [=] { return GetEst().cross_delay_rate; }),
      Log("est_spike_delay", [=] { return GetEst().spike_delay_rate; }),
      Log("est_pre_buffer", [=] { return GetEst().pre_link_buffer_delay; }),
      Log("est_post_buffer", [=] { return GetEst().post_link_buffer_delay; }),
      Log("est_propagation", [=] { return GetEst().propagation_delay; }),
  });
  return loggers;
}
GoogCcStatePrinter::~GoogCcStatePrinter() = default;

void GoogCcStatePrinter::Attach(GoogCcNetworkController* controller) {
  controller_ = controller;
}

bool GoogCcStatePrinter::Attached() const {
  return controller_ != nullptr;
}

void GoogCcStatePrinter::PrintHeaders(RtcEventLogOutput* out) {
  int ix = 0;
  for (const auto& logger : loggers_) {
    if (ix++)
      out->Write(" ");
    out->Write(logger->name());
  }
}

void GoogCcStatePrinter::PrintValues(RtcEventLogOutput* out) {
  RTC_CHECK(controller_);
  int ix = 0;
  for (const auto& logger : loggers_) {
    if (ix++)
      out->Write(" ");
    logger->WriteValue(out);
  }
}

NetworkControlUpdate GoogCcStatePrinter::GetState(Timestamp at_time) const {
  RTC_CHECK(controller_);
  return controller_->GetNetworkState(at_time);
}

GoogCcDebugFactory::GoogCcDebugFactory(GoogCcStatePrinter* printer)
    : printer_(printer) {}

std::unique_ptr<NetworkControllerInterface> GoogCcDebugFactory::Create(
    NetworkControllerConfig config) {
  RTC_CHECK(controller_ == nullptr);
  auto controller = GoogCcNetworkControllerFactory::Create(config);
  controller_ = static_cast<GoogCcNetworkController*>(controller.get());
  printer_->Attach(controller_);
  return controller;
}

}  // namespace webrtc
