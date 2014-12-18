/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_

#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"

#include "webrtc/modules/audio_coding/codecs/isac/main/interface/isac.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

const int kIsacPayloadType = 103;

inline int DivExact(int a, int b) {
  CHECK_EQ(a % b, 0);
  return a / b;
}

template <typename T>
AudioEncoderDecoderIsacT<T>::Config::Config()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      frame_size_ms(30),
      bit_rate(32000) {
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::Config::IsOk() const {
  switch (sample_rate_hz) {
    case 16000:
      return (frame_size_ms == 30 || frame_size_ms == 60) &&
             bit_rate >= 10000 && bit_rate <= 32000;
    case 32000:
      return T::has_32kHz &&
             (frame_size_ms == 30 && bit_rate >= 10000 && bit_rate <= 56000);
    default:
      return false;
  }
}

template <typename T>
AudioEncoderDecoderIsacT<T>::ConfigAdaptive::ConfigAdaptive()
    : payload_type(kIsacPayloadType),
      sample_rate_hz(16000),
      initial_frame_size_ms(30),
      initial_bit_rate(32000),
      enforce_frame_size(false) {
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::ConfigAdaptive::IsOk() const {
  static const int max_rate = T::has_32kHz ? 56000 : 32000;
  return sample_rate_hz == 16000 &&
         (initial_frame_size_ms == 30 || initial_frame_size_ms == 60) &&
         initial_bit_rate >= 10000 && initial_bit_rate <= max_rate;
}

template <typename T>
AudioEncoderDecoderIsacT<T>::AudioEncoderDecoderIsacT(const Config& config)
    : payload_type_(config.payload_type),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false),
      first_output_frame_(true) {
  CHECK(config.IsOk());
  CHECK_EQ(0, T::Create(&isac_state_));
  CHECK_EQ(0, T::EncoderInit(isac_state_, 1));
  CHECK_EQ(0, T::SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, T::Control(isac_state_, config.bit_rate, config.frame_size_ms));
  CHECK_EQ(0, T::SetDecSampRate(isac_state_, config.sample_rate_hz));
}

template <typename T>
AudioEncoderDecoderIsacT<T>::AudioEncoderDecoderIsacT(
    const ConfigAdaptive& config)
    : payload_type_(config.payload_type),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_in_progress_(false),
      first_output_frame_(true) {
  CHECK(config.IsOk());
  CHECK_EQ(0, T::Create(&isac_state_));
  CHECK_EQ(0, T::EncoderInit(isac_state_, 0));
  CHECK_EQ(0, T::SetEncSampRate(isac_state_, config.sample_rate_hz));
  CHECK_EQ(0, T::ControlBwe(isac_state_, config.initial_bit_rate,
                            config.initial_frame_size_ms,
                            config.enforce_frame_size));
  CHECK_EQ(0, T::SetDecSampRate(isac_state_, config.sample_rate_hz));
}

template <typename T>
AudioEncoderDecoderIsacT<T>::~AudioEncoderDecoderIsacT() {
  CHECK_EQ(0, T::Free(isac_state_));
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::sample_rate_hz() const {
  CriticalSectionScoped cs(lock_.get());
  return T::EncSampRate(isac_state_);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::num_channels() const {
  return 1;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Num10MsFramesInNextPacket() const {
  CriticalSectionScoped cs(lock_.get());
  const int samples_in_next_packet = T::GetNewFrameLen(isac_state_);
  return DivExact(samples_in_next_packet, DivExact(sample_rate_hz(), 100));
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Max10MsFramesInAPacket() const {
  return 6;  // iSAC puts at most 60 ms in a packet.
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::EncodeInternal(uint32_t timestamp,
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
    r = T::Encode(isac_state_, audio, encoded);
  }
  if (r < 0) {
    // An error occurred; propagate it to the caller.
    packet_in_progress_ = false;
    return false;
  }

  // T::Encode doesn't allow us to tell it the size of the output
  // buffer. All we can do is check for an overrun after the fact.
  CHECK(static_cast<size_t>(r) <= max_encoded_bytes);

  info->encoded_bytes = r;
  if (r == 0)
    return true;

  // Got enough input to produce a packet. Return the saved timestamp from
  // the first chunk of input that went into the packet.
  packet_in_progress_ = false;
  info->encoded_timestamp = packet_timestamp_;
  info->payload_type = payload_type_;

  if (!T::has_redundant_encoder)
    return true;

  if (first_output_frame_) {
    // Do not emit the first output frame when using redundant encoding.
    info->encoded_bytes = 0;
    first_output_frame_ = false;
  } else {
    // Call the encoder's method to get redundant encoding.
    const size_t primary_length = info->encoded_bytes;
    int16_t secondary_len;
    {
      CriticalSectionScoped cs(lock_.get());
      secondary_len = T::GetRedPayload(isac_state_, &encoded[primary_length]);
    }
    DCHECK_GE(secondary_len, 0);
    // |info| will be implicitly cast to an EncodedInfoLeaf struct, effectively
    // discarding the (empty) vector of redundant information. This is
    // intentional.
    info->redundant.push_back(*info);
    EncodedInfoLeaf secondary_info;
    secondary_info.payload_type = info->payload_type;
    secondary_info.encoded_bytes = secondary_len;
    secondary_info.encoded_timestamp = last_encoded_timestamp_;
    info->redundant.push_back(secondary_info);
    info->encoded_bytes += secondary_len;  // Sum of primary and secondary.
  }
  last_encoded_timestamp_ = packet_timestamp_;
  return true;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Decode(const uint8_t* encoded,
                                        size_t encoded_len,
                                        int16_t* decoded,
                                        SpeechType* speech_type) {
  CriticalSectionScoped cs(lock_.get());
  int16_t temp_type = 1;  // Default is speech.
  int16_t ret =
      T::Decode(isac_state_, encoded, static_cast<int16_t>(encoded_len),
                decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::DecodeRedundant(const uint8_t* encoded,
                                                 size_t encoded_len,
                                                 int16_t* decoded,
                                                 SpeechType* speech_type) {
  CriticalSectionScoped cs(lock_.get());
  int16_t temp_type = 1;  // Default is speech.
  int16_t ret =
      T::DecodeRcu(isac_state_, encoded, static_cast<int16_t>(encoded_len),
                   decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

template <typename T>
bool AudioEncoderDecoderIsacT<T>::HasDecodePlc() const {
  return true;
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::DecodePlc(int num_frames, int16_t* decoded) {
  CriticalSectionScoped cs(lock_.get());
  return T::DecodePlc(isac_state_, decoded, num_frames);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::Init() {
  CriticalSectionScoped cs(lock_.get());
  return T::DecoderInit(isac_state_);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::IncomingPacket(const uint8_t* payload,
                                                size_t payload_len,
                                                uint16_t rtp_sequence_number,
                                                uint32_t rtp_timestamp,
                                                uint32_t arrival_timestamp) {
  CriticalSectionScoped cs(lock_.get());
  return T::UpdateBwEstimate(
      isac_state_, payload, static_cast<int32_t>(payload_len),
      rtp_sequence_number, rtp_timestamp, arrival_timestamp);
}

template <typename T>
int AudioEncoderDecoderIsacT<T>::ErrorCode() {
  CriticalSectionScoped cs(lock_.get());
  return T::GetErrorCode(isac_state_);
}

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_
