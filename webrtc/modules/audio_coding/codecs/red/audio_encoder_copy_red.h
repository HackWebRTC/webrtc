/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_RED_AUDIO_ENCODER_COPY_RED_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_RED_AUDIO_ENCODER_COPY_RED_H_

#include <vector>

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// This class implements redundant audio coding. The class object will have an
// underlying AudioEncoder object that performs the actual encodings. The
// current class will gather the two latest encodings from the underlying codec
// into one packet.
class AudioEncoderCopyRed : public AudioEncoder {
 public:
  struct Config {
   public:
    int payload_type;
    AudioEncoder* speech_encoder;
  };

  // Caller keeps ownership of the AudioEncoder object.
  explicit AudioEncoderCopyRed(const Config& config);

  virtual ~AudioEncoderCopyRed();

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
  AudioEncoder* speech_encoder_;
  int red_payload_type_;
  scoped_ptr<uint8_t[]> secondary_encoded_;
  size_t secondary_allocated_;
  EncodedInfoLeaf secondary_info_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_RED_AUDIO_ENCODER_COPY_RED_H_
