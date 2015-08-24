/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/isac/audio_encoder_isac_t_impl.h"

namespace webrtc {

// Explicit instantiation:
template class AudioEncoderIsacT<IsacFloat>;
template class AudioDecoderIsacT<IsacFloat>;

namespace {
AudioEncoderIsac::Config CreateConfig(const CodecInst& codec_inst,
                                      LockedIsacBandwidthInfo* bwinfo) {
  AudioEncoderIsac::Config config;
  config.bwinfo = bwinfo;
  config.payload_type = codec_inst.pltype;
  config.sample_rate_hz = codec_inst.plfreq;
  config.frame_size_ms =
      rtc::CheckedDivExact(1000 * codec_inst.pacsize, config.sample_rate_hz);
  if (codec_inst.rate != -1)
    config.bit_rate = codec_inst.rate;
  config.adaptive_mode = (codec_inst.rate == -1);
  return config;
}
}  // namespace

AudioEncoderMutableIsacFloat::AudioEncoderMutableIsacFloat(
    const CodecInst& codec_inst,
    LockedIsacBandwidthInfo* bwinfo)
    : AudioEncoderMutableImpl<AudioEncoderIsac>(
          CreateConfig(codec_inst, bwinfo)) {
}

void AudioEncoderMutableIsacFloat::SetMaxPayloadSize(
    int max_payload_size_bytes) {
  auto conf = config();
  conf.max_payload_size_bytes = max_payload_size_bytes;
  Reconstruct(conf);
}

void AudioEncoderMutableIsacFloat::SetMaxRate(int max_rate_bps) {
  auto conf = config();
  conf.max_bit_rate = max_rate_bps;
  Reconstruct(conf);
}

}  // namespace webrtc
