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

#include "webrtc/typedefs.h"

namespace webrtc {

// This is the interface class for encoders in AudioCoding module. Each codec
// type must have an implementation of this class.
class AudioEncoder {
 public:
  struct EncodedInfoLeaf {
    EncodedInfoLeaf()
        : encoded_bytes(0),
          encoded_timestamp(0),
          payload_type(0),
          send_even_if_empty(false),
          speech(true) {}

    size_t encoded_bytes;
    uint32_t encoded_timestamp;
    int payload_type;
    bool send_even_if_empty;
    bool speech;
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
  // The encoder produces zero or more bytes of output in |encoded| and
  // returns additional encoding information.
  // The caller is responsible for making sure that |max_encoded_bytes| is
  // not smaller than the number of bytes actually produced by the encoder.
  EncodedInfo Encode(uint32_t rtp_timestamp,
                     const int16_t* audio,
                     size_t num_samples_per_channel,
                     size_t max_encoded_bytes,
                     uint8_t* encoded);

  // Return the input sample rate in Hz and the number of input channels.
  // These are constants set at instantiation time.
  virtual int SampleRateHz() const = 0;
  virtual int NumChannels() const = 0;

  // Return the maximum number of bytes that can be produced by the encoder
  // at each Encode() call. The caller can use the return value to determine
  // the size of the buffer that needs to be allocated. This value is allowed
  // to depend on encoder parameters like bitrate, frame size etc., so if
  // any of these change, the caller of Encode() is responsible for checking
  // that the buffer is large enough by calling MaxEncodedBytes() again.
  virtual size_t MaxEncodedBytes() const = 0;

  // Returns the rate with which the RTP timestamps are updated. By default,
  // this is the same as sample_rate_hz().
  virtual int RtpTimestampRateHz() const;

  // Returns the number of 10 ms frames the encoder will put in the next
  // packet. This value may only change when Encode() outputs a packet; i.e.,
  // the encoder may vary the number of 10 ms frames from packet to packet, but
  // it must decide the length of the next packet no later than when outputting
  // the preceding packet.
  virtual size_t Num10MsFramesInNextPacket() const = 0;

  // Returns the maximum value that can be returned by
  // Num10MsFramesInNextPacket().
  virtual size_t Max10MsFramesInAPacket() const = 0;

  // Returns the current target bitrate in bits/s. The value -1 means that the
  // codec adapts the target automatically, and a current target cannot be
  // provided.
  virtual int GetTargetBitrate() const = 0;

  // Changes the target bitrate. The implementation is free to alter this value,
  // e.g., if the desired value is outside the valid range.
  virtual void SetTargetBitrate(int bits_per_second) {}

  // Tells the implementation what the projected packet loss rate is. The rate
  // is in the range [0.0, 1.0]. This rate is typically used to adjust channel
  // coding efforts, such as FEC.
  virtual void SetProjectedPacketLossRate(double fraction) {}

  // This is the encode function that the inherited classes must implement. It
  // is called from Encode in the base class.
  virtual EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                                     const int16_t* audio,
                                     size_t max_encoded_bytes,
                                     uint8_t* encoded) = 0;
};

class AudioEncoderMutable : public AudioEncoder {
 public:
  enum Application { kApplicationSpeech, kApplicationAudio };

  // Discards unprocessed audio data.
  virtual void Reset() = 0;

  // Enables codec-internal FEC, if the implementation supports it.
  virtual bool SetFec(bool enable) = 0;

  // Enables or disables codec-internal VAD/DTX, if the implementation supports
  // it.
  virtual bool SetDtx(bool enable) = 0;

  // Sets the application mode. The implementation is free to disregard this
  // setting.
  virtual bool SetApplication(Application application) = 0;

  // Sets an upper limit on the payload size produced by the encoder. The
  // implementation is free to disregard this setting.
  virtual void SetMaxPayloadSize(int max_payload_size_bytes) = 0;

  // Sets the maximum rate which the codec may not exceed for any packet.
  virtual void SetMaxRate(int max_rate_bps) = 0;

  // Informs the encoder about the maximum sample rate which the decoder will
  // use when decoding the bitstream. The implementation is free to disregard
  // this hint.
  virtual bool SetMaxPlaybackRate(int frequency_hz) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_H_
