/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_AUDIO_ENCODER_OPUS_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_AUDIO_ENCODER_OPUS_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder_mutable_impl.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

namespace webrtc {

// NOTE: This class has neither ThreadChecker, nor locks. The owner of an
// AudioEncoderOpus object must ensure that it is not accessed concurrently.

class AudioEncoderOpus final : public AudioEncoder {
 public:
  enum ApplicationMode {
    kVoip = 0,
    kAudio = 1,
  };

  struct Config {
    Config();
    bool IsOk() const;
    int frame_size_ms;
    int num_channels;
    int payload_type;
    ApplicationMode application;
    int bitrate_bps;
    bool fec_enabled;
    int max_playback_rate_hz;
    int complexity;
    bool dtx_enabled;
  };

  explicit AudioEncoderOpus(const Config& config);
  ~AudioEncoderOpus() override;

  int SampleRateHz() const override;
  int NumChannels() const override;
  size_t MaxEncodedBytes() const override;
  size_t Num10MsFramesInNextPacket() const override;
  size_t Max10MsFramesInAPacket() const override;
  int GetTargetBitrate() const override;
  void SetTargetBitrate(int bits_per_second) override;
  void SetProjectedPacketLossRate(double fraction) override;

  double packet_loss_rate() const { return packet_loss_rate_; }
  ApplicationMode application() const { return application_; }
  bool dtx_enabled() const { return dtx_enabled_; }

  EncodedInfo EncodeInternal(uint32_t rtp_timestamp,
                             const int16_t* audio,
                             size_t max_encoded_bytes,
                             uint8_t* encoded) override;

 private:
  const size_t num_10ms_frames_per_packet_;
  const int num_channels_;
  const int payload_type_;
  const ApplicationMode application_;
  int bitrate_bps_;
  const bool dtx_enabled_;
  const size_t samples_per_10ms_frame_;
  std::vector<int16_t> input_buffer_;
  OpusEncInst* inst_;
  uint32_t first_timestamp_in_buffer_;
  double packet_loss_rate_;
};

struct CodecInst;

class AudioEncoderMutableOpus
    : public AudioEncoderMutableImpl<AudioEncoderOpus> {
 public:
  explicit AudioEncoderMutableOpus(const CodecInst& codec_inst);
  bool SetFec(bool enable) override;

  // Set Opus DTX. Once enabled, Opus stops transmission, when it detects voice
  // being inactive. During that, it still sends 2 packets (one for content, one
  // for signaling) about every 400 ms.
  bool SetDtx(bool enable) override;

  bool SetApplication(Application application) override;
  bool SetMaxPlaybackRate(int frequency_hz) override;
  AudioEncoderOpus::ApplicationMode application() const {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder()->application();
  }
  double packet_loss_rate() const {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder()->packet_loss_rate();
  }
  bool dtx_enabled() const {
    CriticalSectionScoped cs(encoder_lock_.get());
    return encoder()->dtx_enabled();
  }
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_AUDIO_ENCODER_OPUS_H_
