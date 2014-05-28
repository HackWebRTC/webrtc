/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include "webrtc/modules/audio_coding/neteq4/tools/neteq_quality_test.h"

namespace webrtc {
namespace test {

const uint8_t kPayloadType = 95;
const int kOutputSizeMs = 10;

NetEqQualityTest::NetEqQualityTest(int block_duration_ms,
                                   int in_sampling_khz,
                                   int out_sampling_khz,
                                   enum NetEqDecoder decoder_type,
                                   int channels,
                                   double drift_factor,
                                   std::string in_filename,
                                   std::string out_filename)
    : decoded_time_ms_(0),
      decodable_time_ms_(0),
      drift_factor_(drift_factor),
      block_duration_ms_(block_duration_ms),
      in_sampling_khz_(in_sampling_khz),
      out_sampling_khz_(out_sampling_khz),
      decoder_type_(decoder_type),
      channels_(channels),
      in_filename_(in_filename),
      out_filename_(out_filename),
      in_size_samples_(in_sampling_khz_ * block_duration_ms_),
      out_size_samples_(out_sampling_khz_ * kOutputSizeMs),
      payload_size_bytes_(0),
      max_payload_bytes_(0),
      in_file_(new InputAudioFile(in_filename_)),
      out_file_(NULL),
      rtp_generator_(new RtpGenerator(in_sampling_khz_, 0, 0,
                                      decodable_time_ms_)) {
  NetEq::Config config;
  config.sample_rate_hz = out_sampling_khz_ * 1000;
  neteq_.reset(NetEq::Create(config));
  max_payload_bytes_ = in_size_samples_ * channels_ * sizeof(int16_t);
  in_data_.reset(new int16_t[in_size_samples_ * channels_]);
  payload_.reset(new uint8_t[max_payload_bytes_]);
  out_data_.reset(new int16_t[out_size_samples_ * channels_]);
}

void NetEqQualityTest::SetUp() {
  out_file_ = fopen(out_filename_.c_str(), "wb");
  ASSERT_TRUE(out_file_ != NULL);
  ASSERT_EQ(0, neteq_->RegisterPayloadType(decoder_type_, kPayloadType));
  rtp_generator_->set_drift_factor(drift_factor_);
}

void NetEqQualityTest::TearDown() {
  fclose(out_file_);
}

int NetEqQualityTest::Transmit() {
  int packet_input_time_ms =
      rtp_generator_->GetRtpHeader(kPayloadType, in_size_samples_,
                                   &rtp_header_);
  if (!PacketLost(packet_input_time_ms) && payload_size_bytes_ > 0) {
    int ret = neteq_->InsertPacket(rtp_header_, &payload_[0],
                                   payload_size_bytes_,
                                   packet_input_time_ms * in_sampling_khz_);
    if (ret != NetEq::kOK)
      return -1;
  }
  return packet_input_time_ms;
}

int NetEqQualityTest::DecodeBlock() {
  int channels;
  int samples;
  int ret = neteq_->GetAudio(out_size_samples_ * channels_, &out_data_[0],
                             &samples, &channels, NULL);

  if (ret != NetEq::kOK) {
    return -1;
  } else {
    assert(channels == channels_);
    assert(samples == kOutputSizeMs * out_sampling_khz_);
    fwrite(&out_data_[0], sizeof(int16_t), samples * channels, out_file_);
    return samples;
  }
}

void NetEqQualityTest::Simulate(int end_time_ms) {
  int audio_size_samples;

  while (decoded_time_ms_ < end_time_ms) {
    while (decodable_time_ms_ - kOutputSizeMs < decoded_time_ms_) {
      ASSERT_TRUE(in_file_->Read(in_size_samples_ * channels_, &in_data_[0]));
      payload_size_bytes_ = EncodeBlock(&in_data_[0],
                                        in_size_samples_, &payload_[0],
                                        max_payload_bytes_);
      decodable_time_ms_ = Transmit() + block_duration_ms_;
    }
    audio_size_samples = DecodeBlock();
    if (audio_size_samples > 0) {
      decoded_time_ms_ += audio_size_samples / out_sampling_khz_;
    }
  }
}

}  // namespace test
}  // namespace webrtc
