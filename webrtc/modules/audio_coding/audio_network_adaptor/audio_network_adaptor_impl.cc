/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"

namespace webrtc {

AudioNetworkAdaptorImpl::Config::Config() = default;

AudioNetworkAdaptorImpl::Config::~Config() = default;

AudioNetworkAdaptorImpl::AudioNetworkAdaptorImpl(
    const Config& config,
    std::unique_ptr<ControllerManager> controller_manager)
    : config_(config), controller_manager_(std::move(controller_manager)) {
  RTC_DCHECK(controller_manager_);
}

AudioNetworkAdaptorImpl::~AudioNetworkAdaptorImpl() = default;

void AudioNetworkAdaptorImpl::SetUplinkBandwidth(int uplink_bandwidth_bps) {
  last_metrics_.uplink_bandwidth_bps = rtc::Optional<int>(uplink_bandwidth_bps);

  // TODO(minyue): Add debug dumping.
}

void AudioNetworkAdaptorImpl::SetUplinkPacketLossFraction(
    float uplink_packet_loss_fraction) {
  last_metrics_.uplink_packet_loss_fraction =
      rtc::Optional<float>(uplink_packet_loss_fraction);

  // TODO(minyue): Add debug dumping.
}

AudioNetworkAdaptor::EncoderRuntimeConfig
AudioNetworkAdaptorImpl::GetEncoderRuntimeConfig() {
  EncoderRuntimeConfig config;
  for (auto& controller :
       controller_manager_->GetSortedControllers(last_metrics_))
    controller->MakeDecision(last_metrics_, &config);

  // TODO(minyue): Add debug dumping.

  return config;
}

void AudioNetworkAdaptorImpl::SetReceiverFrameLengthRange(
    int min_frame_length_ms,
    int max_frame_length_ms) {
  Controller::Constraints constraints;
  constraints.receiver_frame_length_range =
      rtc::Optional<Controller::Constraints::FrameLengthRange>(
          Controller::Constraints::FrameLengthRange(min_frame_length_ms,
                                                    max_frame_length_ms));
  auto controllers = controller_manager_->GetControllers();
  for (auto& controller : controllers)
    controller->SetConstraints(constraints);
}

void AudioNetworkAdaptorImpl::StartDebugDump(FILE* file_handle) {
  // TODO(minyue): Implement this method.
}

}  // namespace webrtc
