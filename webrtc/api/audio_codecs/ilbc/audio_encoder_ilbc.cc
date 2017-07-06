/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/ilbc/audio_encoder_ilbc.h"

#include <memory>
#include <vector>

#include "webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/rtc_base/safe_conversions.h"

namespace webrtc {
namespace {
int GetIlbcBitrate(int ptime) {
  switch (ptime) {
    case 20:
    case 40:
      // 38 bytes per frame of 20 ms => 15200 bits/s.
      return 15200;
    case 30:
    case 60:
      // 50 bytes per frame of 30 ms => (approx) 13333 bits/s.
      return 13333;
    default:
      FATAL();
  }
}
}  // namespace

rtc::Optional<AudioEncoderIlbcConfig> AudioEncoderIlbc::SdpToConfig(
    const SdpAudioFormat& format) {
  return AudioEncoderIlbcImpl::SdpToConfig(format);
}

void AudioEncoderIlbc::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  const SdpAudioFormat fmt = {"ILBC", 8000, 1};
  const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
  specs->push_back({fmt, info});
}

AudioCodecInfo AudioEncoderIlbc::QueryAudioEncoder(
    const AudioEncoderIlbcConfig& config) {
  RTC_DCHECK(config.IsOk());
  return {8000, 1, GetIlbcBitrate(config.frame_size_ms)};
}

std::unique_ptr<AudioEncoder> AudioEncoderIlbc::MakeAudioEncoder(
    const AudioEncoderIlbcConfig& config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  return rtc::MakeUnique<AudioEncoderIlbcImpl>(config, payload_type);
}

}  // namespace webrtc
