/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_G722_INCLUDE_AUDIO_ENCODER_G722_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_G722_INCLUDE_AUDIO_ENCODER_G722_H_

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/g722/include/g722_interface.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class AudioEncoderG722 : public AudioEncoder {
 public:
  struct Config {
    Config() : payload_type(9), frame_size_ms(20), num_channels(1) {}

    int payload_type;
    int frame_size_ms;
    int num_channels;
  };

  explicit AudioEncoderG722(const Config& config);
  virtual ~AudioEncoderG722();

  virtual int sample_rate_hz() const OVERRIDE;
  virtual int num_channels() const OVERRIDE;
  virtual int Num10MsFramesInNextPacket() const OVERRIDE;
  virtual int Max10MsFramesInAPacket() const OVERRIDE;

 protected:
  virtual bool EncodeInternal(uint32_t timestamp,
                              const int16_t* audio,
                              size_t max_encoded_bytes,
                              uint8_t* encoded,
                              EncodedInfo* info) OVERRIDE;

 private:
  // The encoder state for one channel.
  struct EncoderState {
    G722EncInst* encoder;
    scoped_ptr<int16_t[]> speech_buffer;  // Queued up for encoding.
    scoped_ptr<uint8_t[]> encoded_buffer;  // Already encoded.
    EncoderState();
    ~EncoderState();
  };

  const int num_channels_;
  const int payload_type_;
  const int num_10ms_frames_per_packet_;
  int num_10ms_frames_buffered_;
  uint32_t first_timestamp_in_buffer_;
  const scoped_ptr<EncoderState[]> encoders_;
  const scoped_ptr<uint8_t[]> interleave_buffer_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_G722_INCLUDE_AUDIO_ENCODER_G722_H_
