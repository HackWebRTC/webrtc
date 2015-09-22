/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_DECODER_PCM_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_DECODER_PCM_H_

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"

namespace webrtc {

class AudioDecoderPcmU : public AudioDecoder {
 public:
  AudioDecoderPcmU() {}
  void Reset() override;
  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;
  size_t Channels() const override;

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmU);
};

class AudioDecoderPcmA : public AudioDecoder {
 public:
  AudioDecoderPcmA() {}
  void Reset() override;
  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;
  size_t Channels() const override;

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmA);
};

class AudioDecoderPcmUMultiCh : public AudioDecoderPcmU {
 public:
  explicit AudioDecoderPcmUMultiCh(size_t channels)
      : AudioDecoderPcmU(), channels_(channels) {
    RTC_DCHECK_GT(channels, 0u);
  }
  size_t Channels() const override;

 private:
  const size_t channels_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmUMultiCh);
};

class AudioDecoderPcmAMultiCh : public AudioDecoderPcmA {
 public:
  explicit AudioDecoderPcmAMultiCh(size_t channels)
      : AudioDecoderPcmA(), channels_(channels) {
    RTC_DCHECK_GT(channels, 0u);
  }
  size_t Channels() const override;

 private:
  const size_t channels_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmAMultiCh);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_G711_INCLUDE_AUDIO_DECODER_PCM_H_
