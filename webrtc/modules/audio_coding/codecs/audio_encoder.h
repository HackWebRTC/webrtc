/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_H_

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This is the interface class for encoders in AudioCoding module. Each codec
// codec type must have an implementation of this class.
class AudioEncoder {
 public:
  virtual ~AudioEncoder() {}

  // Accepts one 10 ms block of input audio (i.e., sample_rate_hz() / 100 *
  // num_channels() samples). Multi-channel audio must be sample-interleaved.
  // If successful, the encoder produces zero or more bytes of output in
  // |encoded|, and provides the number of encoded bytes in |encoded_bytes|.
  // In case of error, false is returned, otherwise true. It is an error for the
  // encoder to attempt to produce more than |max_encoded_bytes| bytes of
  // output.
  bool Encode(uint32_t timestamp,
              const int16_t* audio,
              size_t num_samples,
              size_t max_encoded_bytes,
              uint8_t* encoded,
              size_t* encoded_bytes,
              uint32_t* encoded_timestamp) {
    CHECK_EQ(num_samples,
             static_cast<size_t>(sample_rate_hz() / 100 * num_channels()));
    bool ret = Encode(timestamp,
                      audio,
                      max_encoded_bytes,
                      encoded,
                      encoded_bytes,
                      encoded_timestamp);
    CHECK_LE(*encoded_bytes, max_encoded_bytes);
    return ret;
  }

  // Returns the input sample rate in Hz, the number of input channels, and the
  // number of 10 ms frames the encoder puts in one output packet. These are
  // constants set at instantiation time.
  virtual int sample_rate_hz() const = 0;
  virtual int num_channels() const = 0;
  virtual int num_10ms_frames_per_packet() const = 0;

 protected:
  virtual bool Encode(uint32_t timestamp,
                      const int16_t* audio,
                      size_t max_encoded_bytes,
                      uint8_t* encoded,
                      size_t* encoded_bytes,
                      uint32_t* encoded_timestamp) = 0;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_H_
