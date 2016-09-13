/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_

#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {

class ControllerManager {
 public:
  virtual ~ControllerManager() = default;

  // Sort controllers based on their significance.
  virtual std::vector<Controller*> GetSortedControllers(
      const Controller::NetworkMetrics& metrics) = 0;

  virtual std::vector<Controller*> GetControllers() const = 0;
};

class ControllerManagerImpl final : public ControllerManager {
 public:
  struct Config {
    Config();
    ~Config();
  };

  explicit ControllerManagerImpl(const Config& config);

  // Dependency injection for testing.
  ControllerManagerImpl(const Config& config,
                        std::vector<std::unique_ptr<Controller>> controllers);

  ~ControllerManagerImpl() override;

  // Sort controllers based on their significance.
  std::vector<Controller*> GetSortedControllers(
      const Controller::NetworkMetrics& metrics) override;

  std::vector<Controller*> GetControllers() const override;

 private:
  const Config config_;

  std::vector<std::unique_ptr<Controller>> controllers_;

  std::vector<Controller*> default_sorted_controllers_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ControllerManagerImpl);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_
