/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/ilbc.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class AudioEncoderIlbc : public AudioEncoder {
 public:
  struct Config {
    Config() : payload_type(102), frame_size_ms(30) {}

    int payload_type;
    int frame_size_ms;
  };

  explicit AudioEncoderIlbc(const Config& config);
  virtual ~AudioEncoderIlbc();

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
  static const int kMaxSamplesPerPacket = 240;
  const int payload_type_;
  const int num_10ms_frames_per_packet_;
  int num_10ms_frames_buffered_;
  uint32_t first_timestamp_in_buffer_;
  int16_t input_buffer_[kMaxSamplesPerPacket];
  IlbcEncoderInstance* encoder_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_INTERFACE_AUDIO_ENCODER_ILBC_H_
