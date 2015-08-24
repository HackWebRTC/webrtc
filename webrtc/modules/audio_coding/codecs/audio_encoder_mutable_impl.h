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
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {

// This is a convenient base class for implementations of AudioEncoderMutable.
// T is the type of the encoder state; it has to look like an AudioEncoder
// subclass whose constructor takes a single T::Config parameter. If P is
// given, this class will inherit from it instead of directly from
// AudioEncoderMutable.
template <typename T, typename P = AudioEncoderMutable>
class AudioEncoderMutableImpl : public P {
 public:
  void Reset() override {
    typename T::Config config;
    {
      CriticalSectionScoped cs(encoder_lock_.get());
      config = config_;
    }
    Reconstruct(config);
  }

  bool SetFec(bool enable) override { return false; }

  bool SetDtx(bool enable) override { return false; }

  bool SetApplication(AudioEncoderMutable::Application application) override {
    return false;
  }

  void SetMaxPayloadSize(int max_payload_size_bytes) override {}

  void SetMaxRate(int max_rate_bps) override {}

  bool SetMaxPlaybackRate(int frequency_hz) override { return false; }

  AudioEncoderMutable::EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                                                  const int16_t* audio,
                                                  size_t max_encoded_bytes,
                                                  uint8_t* encoded) override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->EncodeInternal(rtp_timestamp, audio, max_encoded_bytes,
                                    encoded);
  }
  int SampleRateHz() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->SampleRateHz();
  }
  int NumChannels() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->NumChannels();
  }
  size_t MaxEncodedBytes() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->MaxEncodedBytes();
  }
  int RtpTimestampRateHz() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->RtpTimestampRateHz();
  }
  size_t Num10MsFramesInNextPacket() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->Num10MsFramesInNextPacket();
  }
  size_t Max10MsFramesInAPacket() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->Max10MsFramesInAPacket();
  }
  int GetTargetBitrate() const override {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder_->GetTargetBitrate();
  }
  void SetTargetBitrate(int bits_per_second) override {
    CriticalSectionScoped cs(encoder_lock_.get());
    encoder_->SetTargetBitrate(bits_per_second);
  }
  void SetProjectedPacketLossRate(double fraction) override {
    CriticalSectionScoped cs(encoder_lock_.get());
    encoder_->SetProjectedPacketLossRate(fraction);
  }

 protected:
  explicit AudioEncoderMutableImpl(const typename T::Config& config)
      : encoder_lock_(CriticalSectionWrapper::CreateCriticalSection()) {
    Reconstruct(config);
  }

  bool Reconstruct(const typename T::Config& config) {
    if (!config.IsOk())
      return false;
    CriticalSectionScoped cs(encoder_lock_.get());
    config_ = config;
    encoder_.reset(new T(config_));
    return true;
  }

  typename T::Config config() const {
    CriticalSectionScoped cs(encoder_lock_.get());
    return config_;
  }
  T* encoder() EXCLUSIVE_LOCKS_REQUIRED(encoder_lock_) {
    return encoder_.get();
  }
  const T* encoder() const EXCLUSIVE_LOCKS_REQUIRED(encoder_lock_) {
    return encoder_.get();
  }

  const rtc::scoped_ptr<CriticalSectionWrapper> encoder_lock_;

 private:
  rtc::scoped_ptr<T> encoder_ GUARDED_BY(encoder_lock_);
  typename T::Config config_ GUARDED_BY(encoder_lock_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_ENCODER_MUTABLE_IMPL_H_
