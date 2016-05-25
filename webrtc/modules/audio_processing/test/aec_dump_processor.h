/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_PROCESSOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_PROCESSOR_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/timeutils.h"
#include "webrtc/base/optional.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/audio_file_processor.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/modules/audio_processing/debug.pb.h"
#endif

namespace webrtc {
namespace test {

// Used to read from an aecdump file and write to a WavWriter.
class AecDumpFileProcessor final : public AudioFileProcessor {
 public:
  AecDumpFileProcessor(std::unique_ptr<AudioProcessing> ap,
                       FILE* dump_file,
                       std::string out_filename,
                       std::string reverse_out_filename,
                       rtc::Optional<int> out_sample_rate_hz,
                       rtc::Optional<int> out_num_channels,
                       rtc::Optional<int> reverse_out_sample_rate_hz,
                       rtc::Optional<int> reverse_out_num_channels,
                       bool override_config_message);

  virtual ~AecDumpFileProcessor();

  // Processes the messages in the aecdump file and returns
  // the number of forward stream chunks processed.
  size_t Process(bool verbose_logging) override;

 private:
  void HandleMessage(const webrtc::audioproc::Init& msg);
  void HandleMessage(const webrtc::audioproc::Stream& msg);
  void HandleMessage(const webrtc::audioproc::ReverseStream& msg);
  void HandleMessage(const webrtc::audioproc::Config& msg);

  enum InterfaceType {
    kIntInterface,
    kFloatInterface,
    kNotSpecified,
  };

  std::unique_ptr<AudioProcessing> ap_;
  FILE* dump_file_;
  std::string out_filename_;
  std::string reverse_out_filename_;
  rtc::Optional<int> out_sample_rate_hz_;
  rtc::Optional<int> out_num_channels_;
  rtc::Optional<int> reverse_out_sample_rate_hz_;
  rtc::Optional<int> reverse_out_num_channels_;
  bool override_config_message_;

  std::unique_ptr<ChannelBuffer<float>> in_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_buf_;
  std::unique_ptr<ChannelBuffer<float>> out_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_out_buf_;
  std::unique_ptr<WavWriter> out_file_;
  std::unique_ptr<WavWriter> reverse_out_file_;
  StreamConfig input_config_;
  StreamConfig reverse_config_;
  StreamConfig output_config_;
  StreamConfig reverse_output_config_;
  std::unique_ptr<ChannelBufferWavWriter> buffer_writer_;
  std::unique_ptr<ChannelBufferWavWriter> reverse_buffer_writer_;
  AudioFrame far_frame_;
  AudioFrame near_frame_;
  InterfaceType interface_used_ = InterfaceType::kNotSpecified;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_PROCESSOR_H_
