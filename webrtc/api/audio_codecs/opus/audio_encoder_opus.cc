/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/opus/audio_encoder_opus.h"

#include <memory>
#include <vector>

#include "webrtc/base/ptr_util.h"
#include "webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.h"

namespace webrtc {

rtc::Optional<AudioEncoderOpusConfig> AudioEncoderOpus::SdpToConfig(
    const SdpAudioFormat& format) {
  return AudioEncoderOpusImpl::SdpToConfig(format);
}

void AudioEncoderOpus::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  const SdpAudioFormat fmt = {
      "opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}};
  const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
  specs->push_back({fmt, info});
}

AudioCodecInfo AudioEncoderOpus::QueryAudioEncoder(
    const AudioEncoderOpusConfig& config) {
  RTC_DCHECK(config.IsOk());
  AudioCodecInfo info(48000, config.num_channels, config.bitrate_bps,
                      AudioEncoderOpusConfig::kMinBitrateBps,
                      AudioEncoderOpusConfig::kMaxBitrateBps);
  info.allow_comfort_noise = false;
  info.supports_network_adaption = true;
  return info;
}

std::unique_ptr<AudioEncoder> AudioEncoderOpus::MakeAudioEncoder(
    const AudioEncoderOpusConfig& config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  return rtc::MakeUnique<AudioEncoderOpusImpl>(config, payload_type);
}

}  // namespace webrtc
