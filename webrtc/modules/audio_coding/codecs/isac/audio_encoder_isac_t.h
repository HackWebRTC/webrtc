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

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;

template <typename T>
class AudioEncoderDecoderIsacT : public AudioEncoder, public AudioDecoder {
 public:
  // For constructing an encoder in instantaneous mode. Allowed combinations
  // are
  //  - 16000 Hz, 30 ms, 10000-32000 bps
  //  - 16000 Hz, 60 ms, 10000-32000 bps
  //  - 32000 Hz, 30 ms, 10000-56000 bps (if T has 32 kHz support)
  struct Config {
    Config();
    bool IsOk() const;
    int payload_type;
    int sample_rate_hz;
    int frame_size_ms;
    int bit_rate;  // Limit on the short-term average bit rate, in bits/second.
  };

  // For constructing an encoder in channel-adaptive mode. The sample rate must
  // be 16000 Hz; the initial frame size can be 30 or 60 ms; and the initial
  // bit rate can be 10000-56000 bps if T has 32 kHz support, 10000-32000 bps
  // otherwise.
  struct ConfigAdaptive {
    ConfigAdaptive();
    bool IsOk() const;
    int payload_type;
    int sample_rate_hz;
    int initial_frame_size_ms;
    int initial_bit_rate;
    bool enforce_frame_size;  // Prevent adaptive changes to the frame size?
  };

  explicit AudioEncoderDecoderIsacT(const Config& config);
  explicit AudioEncoderDecoderIsacT(const ConfigAdaptive& config);
  virtual ~AudioEncoderDecoderIsacT() OVERRIDE;

  // AudioEncoder public methods.
  virtual int sample_rate_hz() const OVERRIDE;
  virtual int num_channels() const OVERRIDE;
  virtual int Num10MsFramesInNextPacket() const OVERRIDE;
  virtual int Max10MsFramesInAPacket() const OVERRIDE;

  // AudioDecoder methods.
  virtual int Decode(const uint8_t* encoded,
                     size_t encoded_len,
                     int16_t* decoded,
                     SpeechType* speech_type) OVERRIDE;
  virtual int DecodeRedundant(const uint8_t* encoded,
                              size_t encoded_len,
                              int16_t* decoded,
                              SpeechType* speech_type) OVERRIDE;
  virtual bool HasDecodePlc() const OVERRIDE;
  virtual int DecodePlc(int num_frames, int16_t* decoded) OVERRIDE;
  virtual int Init() OVERRIDE;
  virtual int IncomingPacket(const uint8_t* payload,
                             size_t payload_len,
                             uint16_t rtp_sequence_number,
                             uint32_t rtp_timestamp,
                             uint32_t arrival_timestamp) OVERRIDE;
  virtual int ErrorCode() OVERRIDE;

 protected:
  // AudioEncoder protected method.
  virtual bool EncodeInternal(uint32_t timestamp,
                              const int16_t* audio,
                              size_t max_encoded_bytes,
                              uint8_t* encoded,
                              EncodedInfo* info) OVERRIDE;

 private:
  const int payload_type_;

  // iSAC encoder/decoder state, guarded by a mutex to ensure that encode calls
  // from one thread won't clash with decode calls from another thread.
  const scoped_ptr<CriticalSectionWrapper> lock_;
  typename T::instance_type* isac_state_ GUARDED_BY(lock_);

  // Have we accepted input but not yet emitted it in a packet?
  bool packet_in_progress_;

  // Working on the very first output frame.
  bool first_output_frame_;

  // Timestamp of the first input of the currently in-progress packet.
  uint32_t packet_timestamp_;

  // Timestamp of the previously encoded packet.
  uint32_t last_encoded_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderDecoderIsacT);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_
