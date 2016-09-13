/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_

#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"

namespace webrtc {

class AudioNetworkAdaptorImpl final : public AudioNetworkAdaptor {
 public:
  struct Config {
    Config();
    ~Config();
  };

  AudioNetworkAdaptorImpl(
      const Config& config,
      std::unique_ptr<ControllerManager> controller_manager);

  ~AudioNetworkAdaptorImpl() override;

  void SetUplinkBandwidth(int uplink_bandwidth_bps) override;

  void SetUplinkPacketLossFraction(float uplink_packet_loss_fraction) override;

  void SetReceiverFrameLengthRange(int min_frame_length_ms,
                                   int max_frame_length_ms) override;

  EncoderRuntimeConfig GetEncoderRuntimeConfig() override;

  void StartDebugDump(FILE* file_handle) override;

 private:
  const Config config_;

  std::unique_ptr<ControllerManager> controller_manager_;

  Controller::NetworkMetrics last_metrics_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioNetworkAdaptorImpl);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_AUDIO_NETWORK_ADAPTOR_IMPL_H_
