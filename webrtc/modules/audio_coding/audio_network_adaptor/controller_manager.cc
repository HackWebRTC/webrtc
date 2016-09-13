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

#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"

namespace webrtc {

ControllerManagerImpl::Config::Config() = default;

ControllerManagerImpl::Config::~Config() = default;

ControllerManagerImpl::ControllerManagerImpl(const Config& config)
    : config_(config) {}

ControllerManagerImpl::ControllerManagerImpl(
    const Config& config,
    std::vector<std::unique_ptr<Controller>> controllers)
    : config_(config), controllers_(std::move(controllers)) {
  for (auto& controller : controllers_) {
    default_sorted_controllers_.push_back(controller.get());
  }
}

ControllerManagerImpl::~ControllerManagerImpl() = default;

std::vector<Controller*> ControllerManagerImpl::GetSortedControllers(
    const Controller::NetworkMetrics& metrics) {
  // TODO(minyue): Reorder controllers according to their significance.
  return default_sorted_controllers_;
}

std::vector<Controller*> ControllerManagerImpl::GetControllers() const {
  return default_sorted_controllers_;
}

}  // namespace webrtc
