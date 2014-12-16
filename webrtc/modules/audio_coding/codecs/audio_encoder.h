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
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This is the interface class for encoders in AudioCoding module. Each codec
// type must have an implementation of this class.
class AudioEncoder {
 public:
  struct EncodedInfoLeaf {
    EncodedInfoLeaf()
        : encoded_bytes(0), encoded_timestamp(0), payload_type(0) {}

    size_t encoded_bytes;
    uint32_t encoded_timestamp;
    int payload_type;
  };

  // This is the main struct for auxiliary encoding information. Each encoded
  // packet should be accompanied by one EncodedInfo struct, containing the
  // total number of |encoded_bytes|, the |encoded_timestamp| and the
  // |payload_type|. If the packet contains redundant encodings, the |redundant|
  // vector will be populated with EncodedInfoLeaf structs. Each struct in the
  // vector represents one encoding; the order of structs in the vector is the
  // same as the order in which the actual payloads are written to the byte
  // stream. When EncoderInfoLeaf structs are present in the vector, the main
  // struct's |encoded_bytes| will be the sum of all the |encoded_bytes| in the
  // vector.
  struct EncodedInfo : public EncodedInfoLeaf {
    EncodedInfo();
    ~EncodedInfo();

    std::vector<EncodedInfoLeaf> redundant;
  };

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
              size_t num_samples_per_channel,
              size_t max_encoded_bytes,
              uint8_t* encoded,
              EncodedInfo* info) {
    CHECK_EQ(num_samples_per_channel,
             static_cast<size_t>(sample_rate_hz() / 100));
    bool ret = EncodeInternal(timestamp,
                              audio,
                              max_encoded_bytes,
                              encoded,
                              info);
    CHECK_LE(info->encoded_bytes, max_encoded_bytes);
    return ret;
  }

  // Return the input sample rate in Hz and the number of input channels.
  // These are constants set at instantiation time.
  virtual int sample_rate_hz() const = 0;
  virtual int num_channels() const = 0;

  // Returns the number of 10 ms frames the encoder will put in the next
  // packet. This value may only change when Encode() outputs a packet; i.e.,
  // the encoder may vary the number of 10 ms frames from packet to packet, but
  // it must decide the length of the next packet no later than when outputting
  // the preceding packet.
  virtual int Num10MsFramesInNextPacket() const = 0;

  // Returns the maximum value that can be returned by
  // Num10MsFramesInNextPacket().
  virtual int Max10MsFramesInAPacket() const = 0;

 protected:
  virtual bool EncodeInternal(uint32_t timestamp,
                              const int16_t* audio,
                              size_t max_encoded_bytes,
                              uint8_t* encoded,
                              EncodedInfo* info) = 0;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_H_
