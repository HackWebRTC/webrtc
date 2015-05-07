/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_MUTABLE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_MUTABLE_IMPL_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

namespace webrtc {

// This is a convenient base class for implementations of AudioEncoderMutable.
// T is the type of the encoder state; it has to look like an AudioEncoder
// subclass whose constructor takes a single T::Config parameter. If P is
// given, this class will inherit from it instead of directly from
// AudioEncoderMutable.
template <typename T, typename P = AudioEncoderMutable>
class AudioEncoderMutableImpl : public P {
 public:
  void Reset() override { Reconstruct(config_); }

  bool SetFec(bool enable) override { return false; }

  bool SetDtx(bool enable, bool force) override { return false; }

  bool SetApplication(AudioEncoderMutable::Application application,
                      bool force) override {
    return false;
  }

  void SetMaxPayloadSize(int max_payload_size_bytes) override {}

  void SetMaxRate(int max_rate_bps) override {}

  bool SetMaxPlaybackRate(int frequency_hz) override { return false; }

  AudioEncoderMutable::EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                                                  const int16_t* audio,
                                                  size_t max_encoded_bytes,
                                                  uint8_t* encoded) override {
    return encoder_->EncodeInternal(rtp_timestamp, audio, max_encoded_bytes,
                                    encoded);
  }
  int SampleRateHz() const override { return encoder_->SampleRateHz(); }
  int NumChannels() const override { return encoder_->NumChannels(); }
  size_t MaxEncodedBytes() const override {
    return encoder_->MaxEncodedBytes();
  }
  int RtpTimestampRateHz() const override {
    return encoder_->RtpTimestampRateHz();
  }
  int Num10MsFramesInNextPacket() const override {
    return encoder_->Num10MsFramesInNextPacket();
  }
  int Max10MsFramesInAPacket() const override {
    return encoder_->Max10MsFramesInAPacket();
  }
  void SetTargetBitrate(int bits_per_second) override {
    encoder_->SetTargetBitrate(bits_per_second);
  }
  void SetProjectedPacketLossRate(double fraction) override {
    encoder_->SetProjectedPacketLossRate(fraction);
  }

 protected:
  explicit AudioEncoderMutableImpl(const typename T::Config& config) {
    Reconstruct(config);
  }

  bool Reconstruct(const typename T::Config& config) {
    if (!config.IsOk())
      return false;
    config_ = config;
    encoder_.reset(new T(config_));
    return true;
  }

  const typename T::Config& config() const { return config_; }
  T* encoder() { return encoder_.get(); }
  const T* encoder() const { return encoder_.get(); }

 private:
  rtc::scoped_ptr<T> encoder_;
  typename T::Config config_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_MUTABLE_IMPL_H_
