/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/pacer_controller.h"

#include "modules/congestion_controller/rtp/network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

PacerController::PacerController(PacedSender* pacer) : pacer_(pacer) {
  sequenced_checker_.Detach();
}

PacerController::~PacerController() = default;

void PacerController::OnCongestionWindow(CongestionWindow congestion_window) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (congestion_window.enabled) {
    congestion_window_ = congestion_window;
  } else {
    congestion_window_ = rtc::nullopt;
    congested_ = false;
    UpdatePacerState();
  }
}

void PacerController::OnNetworkAvailability(NetworkAvailability msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  network_available_ = msg.network_available;
  congested_ = false;
  UpdatePacerState();
}

void PacerController::OnNetworkRouteChange(NetworkRouteChange) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  congested_ = false;
  UpdatePacerState();
}

void PacerController::OnPacerConfig(PacerConfig msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  pacer_->SetPacingRates(pacing_rate.bps(), padding_rate.bps());
}

void PacerController::OnProbeClusterConfig(ProbeClusterConfig config) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  int64_t bitrate_bps = config.target_data_rate.bps();
  pacer_->CreateProbeCluster(bitrate_bps);
}

void PacerController::OnOutstandingData(OutstandingData msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (congestion_window_.has_value()) {
    congested_ = msg.in_flight_data > congestion_window_->data_window;
  }
  UpdatePacerState();
}

void PacerController::UpdatePacerState() {
  bool pause = congested_ || !network_available_;
  SetPacerState(pause);
}

void PacerController::SetPacerState(bool paused) {
  if (paused && !pacer_paused_)
    pacer_->Pause();
  else if (!paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = paused;
}

}  // namespace webrtc
