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

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/g711_interface.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/include/audio_decoder_g722.h"
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
  RTC_DCHECK_EQ(sample_rate_hz, 8000);
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
  RTC_DCHECK_EQ(sample_rate_hz, 8000);
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

AudioDecoderCng::AudioDecoderCng() {
  RTC_CHECK_EQ(0, WebRtcCng_CreateDec(&dec_state_));
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
