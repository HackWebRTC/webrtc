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

#include "webrtc/modules/audio_coding/codecs/isac/main/interface/isac.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

namespace {

const int kIsacPayloadType = 103;

int DivExact(int a, int b) {
  CHECK_EQ(a % b, 0);
  return a / b;
}

}  // namespace

AudioEncoderDecoderIsac::Config::Config()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      frame_size_ms(30),
      bit_rate(32000) {}

bool AudioEncoderDecoderIsac::Config::IsOk() const {
  switch (sample_rate_hz) {
    case 16000:
      return (frame_size_ms == 30 || frame_size_ms == 60) &&
             bit_rate >= 10000 && bit_rate <= 32000;
    case 32000:
      return frame_size_ms == 30 && bit_rate >= 10000 && bit_rate <= 56000;
    default:
      return false;
  }
}

AudioEncoderDecoderIsac::ConfigAdaptive::ConfigAdaptive()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      initial_frame_size_ms(30),
      initial_bit_rate(32000),
      enforce_frame_size(false) {}

bool AudioEncoderDecoderIsac::ConfigAdaptive::IsOk() const {
  return sample_rate_hz == 16000 &&
         (initial_frame_size_ms == 30 || initial_frame_size_ms == 60) &&
         initial_bit_rate >= 10000 && initial_bit_rate <= 56000;
}

AudioEncoderDecoderIsac::AudioEncoderDecoderIsac(const Config& config)
    : payload_type_(config.payload_type),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false) {
  CHECK(config.IsOk());
  CHECK_EQ(0, WebRtcIsac_Create(&isac_state_));
  CHECK_EQ(0, WebRtcIsac_EncoderInit(isac_state_, 1));
  CHECK_EQ(0, WebRtcIsac_SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, WebRtcIsac_Control(isac_state_, config.bit_rate,
                                 config.frame_size_ms));
  CHECK_EQ(0, WebRtcIsac_SetDecSampRate(isac_state_, config.sample_rate_hz));
}

AudioEncoderDecoderIsac::AudioEncoderDecoderIsac(const ConfigAdaptive& config)
    : payload_type_(config.payload_type),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false) {
  CHECK(config.IsOk());
  CHECK_EQ(0, WebRtcIsac_Create(&isac_state_));
  CHECK_EQ(0, WebRtcIsac_EncoderInit(isac_state_, 0));
  CHECK_EQ(0, WebRtcIsac_SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, WebRtcIsac_ControlBwe(isac_state_, config.initial_bit_rate,
                                    config.initial_frame_size_ms,
                                    config.enforce_frame_size));
  CHECK_EQ(0, WebRtcIsac_SetDecSampRate(isac_state_, config.sample_rate_hz));
}

AudioEncoderDecoderIsac::~AudioEncoderDecoderIsac() {
  CHECK_EQ(0, WebRtcIsac_Free(isac_state_));
}

int AudioEncoderDecoderIsac::sample_rate_hz() const {
  CriticalSectionScoped cs(lock_.get());
  return WebRtcIsac_EncSampRate(isac_state_);
}

int AudioEncoderDecoderIsac::num_channels() const {
  return 1;
}

int AudioEncoderDecoderIsac::Num10MsFramesInNextPacket() const {
  CriticalSectionScoped cs(lock_.get());
  const int samples_in_next_packet = WebRtcIsac_GetNewFrameLen(isac_state_);
  return DivExact(samples_in_next_packet, DivExact(sample_rate_hz(), 100));
}

int AudioEncoderDecoderIsac::Max10MsFramesInAPacket() const {
  return 6;  // iSAC puts at most 60 ms in a packet.
}

bool AudioEncoderDecoderIsac::EncodeInternal(uint32_t timestamp,
                                             const int16_t* audio,
                                             size_t max_encoded_bytes,
                                             uint8_t* encoded,
                                             EncodedInfo* info) {
  if (!packet_in_progress_) {
    // Starting a new packet; remember the timestamp for later.
    packet_in_progress_ = true;
    packet_timestamp_ = timestamp;
  }
  int r;
  {
    CriticalSectionScoped cs(lock_.get());
    r = WebRtcIsac_Encode(isac_state_, audio, encoded);
  }
  if (r < 0) {
    // An error occurred; propagate it to the caller.
    packet_in_progress_ = false;
    return false;
  }

  // WebRtcIsac_Encode doesn't allow us to tell it the size of the output
  // buffer. All we can do is check for an overrun after the fact.
  CHECK(static_cast<size_t>(r) <= max_encoded_bytes);

  info->encoded_bytes = r;
  if (r > 0) {
    // Got enough input to produce a packet. Return the saved timestamp from
    // the first chunk of input that went into the packet.
    packet_in_progress_ = false;
    info->encoded_timestamp = packet_timestamp_;
    info->payload_type = payload_type_;
  }
  return true;
}

int AudioEncoderDecoderIsac::Decode(const uint8_t* encoded,
                                    size_t encoded_len,
                                    int16_t* decoded,
                                    SpeechType* speech_type) {
  CriticalSectionScoped cs(lock_.get());
  int16_t temp_type = 1;  // Default is speech.
  int16_t ret =
      WebRtcIsac_Decode(isac_state_, encoded, static_cast<int16_t>(encoded_len),
                        decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

int AudioEncoderDecoderIsac::DecodeRedundant(const uint8_t* encoded,
                                             size_t encoded_len,
                                             int16_t* decoded,
                                             SpeechType* speech_type) {
  CriticalSectionScoped cs(lock_.get());
  int16_t temp_type = 1;  // Default is speech.
  int16_t ret = WebRtcIsac_DecodeRcu(isac_state_, encoded,
                                     static_cast<int16_t>(encoded_len), decoded,
                                     &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

bool AudioEncoderDecoderIsac::HasDecodePlc() const { return true; }

int AudioEncoderDecoderIsac::DecodePlc(int num_frames, int16_t* decoded) {
  CriticalSectionScoped cs(lock_.get());
  return WebRtcIsac_DecodePlc(isac_state_, decoded, num_frames);
}

int AudioEncoderDecoderIsac::Init() {
  CriticalSectionScoped cs(lock_.get());
  return WebRtcIsac_DecoderInit(isac_state_);
}

int AudioEncoderDecoderIsac::IncomingPacket(const uint8_t* payload,
                                            size_t payload_len,
                                            uint16_t rtp_sequence_number,
                                            uint32_t rtp_timestamp,
                                            uint32_t arrival_timestamp) {
  CriticalSectionScoped cs(lock_.get());
  return WebRtcIsac_UpdateBwEstimate(
      isac_state_, payload, static_cast<int32_t>(payload_len),
      rtp_sequence_number, rtp_timestamp, arrival_timestamp);
}

int AudioEncoderDecoderIsac::ErrorCode() {
  CriticalSectionScoped cs(lock_.get());
  return WebRtcIsac_GetErrorCode(isac_state_);
}

}  // namespace webrtc
