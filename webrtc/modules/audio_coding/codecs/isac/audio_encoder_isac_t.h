/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_

#include <vector>

#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/isac/locked_bandwidth_info.h"

namespace webrtc {

template <typename T>
class AudioEncoderIsacT final : public AudioEncoder {
 public:
  // Allowed combinations of sample rate, frame size, and bit rate are
  //  - 16000 Hz, 30 ms, 10000-32000 bps
  //  - 16000 Hz, 60 ms, 10000-32000 bps
  //  - 32000 Hz, 30 ms, 10000-56000 bps (if T has super-wideband support)
  //  - 48000 Hz, 30 ms, 10000-56000 bps (if T has super-wideband support)
  struct Config {
    Config();
    bool IsOk() const;

    LockedIsacBandwidthInfo* bwinfo;

    int payload_type;
    int sample_rate_hz;
    int frame_size_ms;
    int bit_rate;  // Limit on the short-term average bit rate, in bits/s.
    int max_payload_size_bytes;
    int max_bit_rate;

    // If true, the encoder will dynamically adjust frame size and bit rate;
    // the configured values are then merely the starting point.
    bool adaptive_mode;

    // In adaptive mode, prevent adaptive changes to the frame size. (Not used
    // in nonadaptive mode.)
    bool enforce_frame_size;
  };

  explicit AudioEncoderIsacT(const Config& config);
  ~AudioEncoderIsacT() override;

  int SampleRateHz() const override;
  int NumChannels() const override;
  size_t MaxEncodedBytes() const override;
  size_t Num10MsFramesInNextPacket() const override;
  size_t Max10MsFramesInAPacket() const override;
  int GetTargetBitrate() const override;
  EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                             const int16_t* audio,
                             size_t max_encoded_bytes,
                             uint8_t* encoded) override;

 private:
  // This value is taken from STREAM_SIZE_MAX_60 for iSAC float (60 ms) and
  // STREAM_MAXW16_60MS for iSAC fix (60 ms).
  static const size_t kSufficientEncodeBufferSizeBytes = 400;

  const int payload_type_;
  typename T::instance_type* isac_state_;
  LockedIsacBandwidthInfo* bwinfo_;

  // Have we accepted input but not yet emitted it in a packet?
  bool packet_in_progress_;

  // Timestamp of the first input of the currently in-progress packet.
  uint32_t packet_timestamp_;

  // Timestamp of the previously encoded packet.
  uint32_t last_encoded_timestamp_;

  const int target_bitrate_bps_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderIsacT);
};

template <typename T>
class AudioDecoderIsacT final : public AudioDecoder {
 public:
  AudioDecoderIsacT();
  explicit AudioDecoderIsacT(LockedIsacBandwidthInfo* bwinfo);
  ~AudioDecoderIsacT() override;

  bool HasDecodePlc() const override;
  size_t DecodePlc(size_t num_frames, int16_t* decoded) override;
  void Reset() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;
  int ErrorCode() override;
  size_t Channels() const override;
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  typename T::instance_type* isac_state_;
  LockedIsacBandwidthInfo* bwinfo_;
  int decoder_sample_rate_hz_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderIsacT);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_
