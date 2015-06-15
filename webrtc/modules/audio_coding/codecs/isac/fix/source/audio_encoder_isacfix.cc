/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/isac/audio_encoder_isac_t_impl.h"

namespace webrtc {

const uint16_t IsacFix::kFixSampleRate;

// Explicit instantiation of AudioEncoderDecoderIsacT<IsacFix>, a.k.a.
// AudioEncoderDecoderIsacFix.
template class AudioEncoderDecoderIsacT<IsacFix>;

namespace {
AudioEncoderDecoderIsacFix::Config CreateConfig(const CodecInst& codec_inst) {
  AudioEncoderDecoderIsacFix::Config config;
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

AudioEncoderDecoderMutableIsacFix::AudioEncoderDecoderMutableIsacFix(
    const CodecInst& codec_inst)
    : AudioEncoderMutableImpl<AudioEncoderDecoderIsacFix,
                              AudioEncoderDecoderMutableIsac>(
          CreateConfig(codec_inst)) {
}

void AudioEncoderDecoderMutableIsacFix::UpdateSettings(
    const CodecInst& codec_inst) {
  bool success = Reconstruct(CreateConfig(codec_inst));
  DCHECK(success);
}

void AudioEncoderDecoderMutableIsacFix::SetMaxPayloadSize(
    int max_payload_size_bytes) {
  auto conf = config();
  conf.max_payload_size_bytes = max_payload_size_bytes;
  Reconstruct(conf);
}

void AudioEncoderDecoderMutableIsacFix::SetMaxRate(int max_rate_bps) {
  auto conf = config();
  conf.max_bit_rate = max_rate_bps;
  Reconstruct(conf);
}

int AudioEncoderDecoderMutableIsacFix::Decode(const uint8_t* encoded,
                                              size_t encoded_len,
                                              int sample_rate_hz,
                                              size_t max_decoded_bytes,
                                              int16_t* decoded,
                                              SpeechType* speech_type) {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->Decode(encoded, encoded_len, sample_rate_hz,
                           max_decoded_bytes, decoded, speech_type);
}

int AudioEncoderDecoderMutableIsacFix::DecodeRedundant(
    const uint8_t* encoded,
    size_t encoded_len,
    int sample_rate_hz,
    size_t max_decoded_bytes,
    int16_t* decoded,
    SpeechType* speech_type) {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->DecodeRedundant(encoded, encoded_len, sample_rate_hz,
                                    max_decoded_bytes, decoded, speech_type);
}

bool AudioEncoderDecoderMutableIsacFix::HasDecodePlc() const {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->HasDecodePlc();
}

int AudioEncoderDecoderMutableIsacFix::DecodePlc(int num_frames,
                                                 int16_t* decoded) {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->DecodePlc(num_frames, decoded);
}

int AudioEncoderDecoderMutableIsacFix::Init() {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->Init();
}

int AudioEncoderDecoderMutableIsacFix::IncomingPacket(
    const uint8_t* payload,
    size_t payload_len,
    uint16_t rtp_sequence_number,
    uint32_t rtp_timestamp,
    uint32_t arrival_timestamp) {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->IncomingPacket(payload, payload_len, rtp_sequence_number,
                                   rtp_timestamp, arrival_timestamp);
}

int AudioEncoderDecoderMutableIsacFix::ErrorCode() {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->ErrorCode();
}

int AudioEncoderDecoderMutableIsacFix::PacketDuration(
    const uint8_t* encoded,
    size_t encoded_len) const {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->PacketDuration(encoded, encoded_len);
}

int AudioEncoderDecoderMutableIsacFix::PacketDurationRedundant(
    const uint8_t* encoded,
    size_t encoded_len) const {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->PacketDurationRedundant(encoded, encoded_len);
}

bool AudioEncoderDecoderMutableIsacFix::PacketHasFec(const uint8_t* encoded,
                                                     size_t encoded_len) const {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->PacketHasFec(encoded, encoded_len);
}

size_t AudioEncoderDecoderMutableIsacFix::Channels() const {
  CriticalSectionScoped cs(encoder_lock_.get());
  return encoder()->Channels();
}

}  // namespace webrtc
