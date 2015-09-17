/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/audio_decoder_impl.h"

#include <assert.h>
#include <string.h>  // memmove

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/g711_interface.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/include/g722_interface.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/audio_decoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_decoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/audio_decoder_pcm16b.h"

namespace webrtc {

// PCMu

void AudioDecoderPcmU::Reset() {
}
size_t AudioDecoderPcmU::Channels() const {
  return 1;
}

int AudioDecoderPcmU::DecodeInternal(const uint8_t* encoded,
                                     size_t encoded_len,
                                     int sample_rate_hz,
                                     int16_t* decoded,
                                     SpeechType* speech_type) {
  DCHECK_EQ(sample_rate_hz, 8000);
  int16_t temp_type = 1;  // Default is speech.
  size_t ret = WebRtcG711_DecodeU(encoded, encoded_len, decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return static_cast<int>(ret);
}

int AudioDecoderPcmU::PacketDuration(const uint8_t* encoded,
                                     size_t encoded_len) const {
  // One encoded byte per sample per channel.
  return static_cast<int>(encoded_len / Channels());
}

size_t AudioDecoderPcmUMultiCh::Channels() const {
  return channels_;
}

// PCMa

void AudioDecoderPcmA::Reset() {
}
size_t AudioDecoderPcmA::Channels() const {
  return 1;
}

int AudioDecoderPcmA::DecodeInternal(const uint8_t* encoded,
                                     size_t encoded_len,
                                     int sample_rate_hz,
                                     int16_t* decoded,
                                     SpeechType* speech_type) {
  DCHECK_EQ(sample_rate_hz, 8000);
  int16_t temp_type = 1;  // Default is speech.
  size_t ret = WebRtcG711_DecodeA(encoded, encoded_len, decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return static_cast<int>(ret);
}

int AudioDecoderPcmA::PacketDuration(const uint8_t* encoded,
                                     size_t encoded_len) const {
  // One encoded byte per sample per channel.
  return static_cast<int>(encoded_len / Channels());
}

size_t AudioDecoderPcmAMultiCh::Channels() const {
  return channels_;
}

// G.722
#ifdef WEBRTC_CODEC_G722
AudioDecoderG722::AudioDecoderG722() {
  WebRtcG722_CreateDecoder(&dec_state_);
  WebRtcG722_DecoderInit(dec_state_);
}

AudioDecoderG722::~AudioDecoderG722() {
  WebRtcG722_FreeDecoder(dec_state_);
}

bool AudioDecoderG722::HasDecodePlc() const {
  return false;
}

int AudioDecoderG722::DecodeInternal(const uint8_t* encoded,
                                     size_t encoded_len,
                                     int sample_rate_hz,
                                     int16_t* decoded,
                                     SpeechType* speech_type) {
  DCHECK_EQ(sample_rate_hz, 16000);
  int16_t temp_type = 1;  // Default is speech.
  size_t ret =
      WebRtcG722_Decode(dec_state_, encoded, encoded_len, decoded, &temp_type);
  *speech_type = ConvertSpeechType(temp_type);
  return static_cast<int>(ret);
}

void AudioDecoderG722::Reset() {
  WebRtcG722_DecoderInit(dec_state_);
}

int AudioDecoderG722::PacketDuration(const uint8_t* encoded,
                                     size_t encoded_len) const {
  // 1/2 encoded byte per sample per channel.
  return static_cast<int>(2 * encoded_len / Channels());
}

size_t AudioDecoderG722::Channels() const {
  return 1;
}

AudioDecoderG722Stereo::AudioDecoderG722Stereo() {
  WebRtcG722_CreateDecoder(&dec_state_left_);
  WebRtcG722_CreateDecoder(&dec_state_right_);
  WebRtcG722_DecoderInit(dec_state_left_);
  WebRtcG722_DecoderInit(dec_state_right_);
}

AudioDecoderG722Stereo::~AudioDecoderG722Stereo() {
  WebRtcG722_FreeDecoder(dec_state_left_);
  WebRtcG722_FreeDecoder(dec_state_right_);
}

int AudioDecoderG722Stereo::DecodeInternal(const uint8_t* encoded,
                                           size_t encoded_len,
                                           int sample_rate_hz,
                                           int16_t* decoded,
                                           SpeechType* speech_type) {
  DCHECK_EQ(sample_rate_hz, 16000);
  int16_t temp_type = 1;  // Default is speech.
  // De-interleave the bit-stream into two separate payloads.
  uint8_t* encoded_deinterleaved = new uint8_t[encoded_len];
  SplitStereoPacket(encoded, encoded_len, encoded_deinterleaved);
  // Decode left and right.
  size_t decoded_len = WebRtcG722_Decode(dec_state_left_, encoded_deinterleaved,
                                         encoded_len / 2, decoded, &temp_type);
  size_t ret = WebRtcG722_Decode(
      dec_state_right_, &encoded_deinterleaved[encoded_len / 2],
      encoded_len / 2, &decoded[decoded_len], &temp_type);
  if (ret == decoded_len) {
    ret += decoded_len;  // Return total number of samples.
    // Interleave output.
    for (size_t k = ret / 2; k < ret; k++) {
        int16_t temp = decoded[k];
        memmove(&decoded[2 * k - ret + 2], &decoded[2 * k - ret + 1],
                (ret - k - 1) * sizeof(int16_t));
        decoded[2 * k - ret + 1] = temp;
    }
  }
  *speech_type = ConvertSpeechType(temp_type);
  delete [] encoded_deinterleaved;
  return static_cast<int>(ret);
}

size_t AudioDecoderG722Stereo::Channels() const {
  return 2;
}

void AudioDecoderG722Stereo::Reset() {
  WebRtcG722_DecoderInit(dec_state_left_);
  WebRtcG722_DecoderInit(dec_state_right_);
}

// Split the stereo packet and place left and right channel after each other
// in the output array.
void AudioDecoderG722Stereo::SplitStereoPacket(const uint8_t* encoded,
                                               size_t encoded_len,
                                               uint8_t* encoded_deinterleaved) {
  assert(encoded);
  // Regroup the 4 bits/sample so |l1 l2| |r1 r2| |l3 l4| |r3 r4| ...,
  // where "lx" is 4 bits representing left sample number x, and "rx" right
  // sample. Two samples fit in one byte, represented with |...|.
  for (size_t i = 0; i + 1 < encoded_len; i += 2) {
    uint8_t right_byte = ((encoded[i] & 0x0F) << 4) + (encoded[i + 1] & 0x0F);
    encoded_deinterleaved[i] = (encoded[i] & 0xF0) + (encoded[i + 1] >> 4);
    encoded_deinterleaved[i + 1] = right_byte;
  }

  // Move one byte representing right channel each loop, and place it at the
  // end of the bytestream vector. After looping the data is reordered to:
  // |l1 l2| |l3 l4| ... |l(N-1) lN| |r1 r2| |r3 r4| ... |r(N-1) r(N)|,
  // where N is the total number of samples.
  for (size_t i = 0; i < encoded_len / 2; i++) {
    uint8_t right_byte = encoded_deinterleaved[i + 1];
    memmove(&encoded_deinterleaved[i + 1], &encoded_deinterleaved[i + 2],
            encoded_len - i - 2);
    encoded_deinterleaved[encoded_len - 1] = right_byte;
  }
}
#endif

AudioDecoderCng::AudioDecoderCng() {
  CHECK_EQ(0, WebRtcCng_CreateDec(&dec_state_));
  WebRtcCng_InitDec(dec_state_);
}

AudioDecoderCng::~AudioDecoderCng() {
  WebRtcCng_FreeDec(dec_state_);
}

void AudioDecoderCng::Reset() {
  WebRtcCng_InitDec(dec_state_);
}

int AudioDecoderCng::IncomingPacket(const uint8_t* payload,
                                    size_t payload_len,
                                    uint16_t rtp_sequence_number,
                                    uint32_t rtp_timestamp,
                                    uint32_t arrival_timestamp) {
  return -1;
}

CNG_dec_inst* AudioDecoderCng::CngDecoderInstance() {
  return dec_state_;
}

size_t AudioDecoderCng::Channels() const {
  return 1;
}

int AudioDecoderCng::DecodeInternal(const uint8_t* encoded,
                                    size_t encoded_len,
                                    int sample_rate_hz,
                                    int16_t* decoded,
                                    SpeechType* speech_type) {
  return -1;
}

bool CodecSupported(NetEqDecoder codec_type) {
  switch (codec_type) {
    case kDecoderPCMu:
    case kDecoderPCMa:
    case kDecoderPCMu_2ch:
    case kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
#endif
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
#endif
#ifdef WEBRTC_CODEC_ISAC
    case kDecoderISACswb:
    case kDecoderISACfb:
#endif
    case kDecoderPCM16B:
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb48kHz:
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16Bwb_2ch:
    case kDecoderPCM16Bswb32kHz_2ch:
    case kDecoderPCM16Bswb48kHz_2ch:
    case kDecoderPCM16B_5ch:
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
    case kDecoderG722_2ch:
#endif
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
    case kDecoderOpus_2ch:
#endif
    case kDecoderRED:
    case kDecoderAVT:
    case kDecoderCNGnb:
    case kDecoderCNGwb:
    case kDecoderCNGswb32kHz:
    case kDecoderCNGswb48kHz:
    case kDecoderArbitrary: {
      return true;
    }
    default: {
      return false;
    }
  }
}

int CodecSampleRateHz(NetEqDecoder codec_type) {
  switch (codec_type) {
    case kDecoderPCMu:
    case kDecoderPCMa:
    case kDecoderPCMu_2ch:
    case kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
#endif
    case kDecoderPCM16B:
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16B_5ch:
    case kDecoderCNGnb: {
      return 8000;
    }
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
#endif
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bwb_2ch:
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
    case kDecoderG722_2ch:
#endif
    case kDecoderCNGwb: {
      return 16000;
    }
#ifdef WEBRTC_CODEC_ISAC
    case kDecoderISACswb:
    case kDecoderISACfb:
#endif
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb32kHz_2ch:
    case kDecoderCNGswb32kHz: {
      return 32000;
    }
    case kDecoderPCM16Bswb48kHz:
    case kDecoderPCM16Bswb48kHz_2ch: {
      return 48000;
    }
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
    case kDecoderOpus_2ch: {
      return 48000;
    }
#endif
    case kDecoderCNGswb48kHz: {
      // TODO(tlegrand): Remove limitation once ACM has full 48 kHz support.
      return 32000;
    }
    default: {
      return -1;  // Undefined sample rate.
    }
  }
}

AudioDecoder* CreateAudioDecoder(NetEqDecoder codec_type) {
  if (!CodecSupported(codec_type)) {
    return NULL;
  }
  switch (codec_type) {
    case kDecoderPCMu:
      return new AudioDecoderPcmU;
    case kDecoderPCMa:
      return new AudioDecoderPcmA;
    case kDecoderPCMu_2ch:
      return new AudioDecoderPcmUMultiCh(2);
    case kDecoderPCMa_2ch:
      return new AudioDecoderPcmAMultiCh(2);
#ifdef WEBRTC_CODEC_ILBC
    case kDecoderILBC:
      return new AudioDecoderIlbc;
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    case kDecoderISAC:
      return new AudioDecoderIsacFix();
#elif defined(WEBRTC_CODEC_ISAC)
    case kDecoderISAC:
    case kDecoderISACswb:
    case kDecoderISACfb:
      return new AudioDecoderIsac();
#endif
    case kDecoderPCM16B:
    case kDecoderPCM16Bwb:
    case kDecoderPCM16Bswb32kHz:
    case kDecoderPCM16Bswb48kHz:
      return new AudioDecoderPcm16B;
    case kDecoderPCM16B_2ch:
    case kDecoderPCM16Bwb_2ch:
    case kDecoderPCM16Bswb32kHz_2ch:
    case kDecoderPCM16Bswb48kHz_2ch:
      return new AudioDecoderPcm16BMultiCh(2);
    case kDecoderPCM16B_5ch:
      return new AudioDecoderPcm16BMultiCh(5);
#ifdef WEBRTC_CODEC_G722
    case kDecoderG722:
      return new AudioDecoderG722;
    case kDecoderG722_2ch:
      return new AudioDecoderG722Stereo;
#endif
#ifdef WEBRTC_CODEC_OPUS
    case kDecoderOpus:
      return new AudioDecoderOpus(1);
    case kDecoderOpus_2ch:
      return new AudioDecoderOpus(2);
#endif
    case kDecoderCNGnb:
    case kDecoderCNGwb:
    case kDecoderCNGswb32kHz:
    case kDecoderCNGswb48kHz:
      return new AudioDecoderCng;
    case kDecoderRED:
    case kDecoderAVT:
    case kDecoderArbitrary:
    default: {
      return NULL;
    }
  }
}

}  // namespace webrtc
