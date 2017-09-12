/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_CONFIG_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_CONFIG_H_

#include "webrtc/api/optional.h"

namespace webrtc {

struct AudioEncoderRuntimeConfig {
  AudioEncoderRuntimeConfig();
  AudioEncoderRuntimeConfig(const AudioEncoderRuntimeConfig& other);
  ~AudioEncoderRuntimeConfig();
  rtc::Optional<int> bitrate_bps;
  rtc::Optional<int> frame_length_ms;
  // Note: This is what we tell the encoder. It doesn't have to reflect
  // the actual NetworkMetrics; it's subject to our decision.
  rtc::Optional<float> uplink_packet_loss_fraction;
  rtc::Optional<bool> enable_fec;
  rtc::Optional<bool> enable_dtx;

  // Some encoders can encode fewer channels than the actual input to make
  // better use of the bandwidth. |num_channels| sets the number of channels
  // to encode.
  rtc::Optional<size_t> num_channels;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_CONFIG_H_
