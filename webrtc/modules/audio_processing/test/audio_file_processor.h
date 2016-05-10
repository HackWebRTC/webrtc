/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AUDIO_FILE_PROCESSOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AUDIO_FILE_PROCESSOR_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "webrtc/base/timeutils.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/modules/audio_processing/debug.pb.h"
#endif

namespace webrtc {

// Holds a few statistics about a series of TickIntervals.
struct TickIntervalStats {
  TickIntervalStats() : min(std::numeric_limits<int64_t>::max()) {}
  int64_t sum;
  int64_t max;
  int64_t min;
};

// Interface for processing an input file with an AudioProcessing instance and
// dumping the results to an output file.
class AudioFileProcessor {
 public:
  static const int kChunksPerSecond = 1000 / AudioProcessing::kChunkSizeMs;

  virtual ~AudioFileProcessor() {}

  // Processes one AudioProcessing::kChunkSizeMs of data from the input file and
  // writes to the output file.
  virtual bool ProcessChunk() = 0;

  // Returns the execution time of all AudioProcessing calls.
  const TickIntervalStats& proc_time() const { return proc_time_; }

 protected:
  // RAII class for execution time measurement. Updates the provided
  // TickIntervalStats based on the time between ScopedTimer creation and
  // leaving the enclosing scope.
  class ScopedTimer {
   public:
    explicit ScopedTimer(TickIntervalStats* proc_time)
      : proc_time_(proc_time), start_time_(rtc::TimeNanos()) {}

    ~ScopedTimer() {
      int64_t interval = rtc::TimeNanos() - start_time_;
      proc_time_->sum += interval;
      proc_time_->max = std::max(proc_time_->max, interval);
      proc_time_->min = std::min(proc_time_->min, interval);
    }

   private:
    TickIntervalStats* const proc_time_;
    int64_t start_time_;
  };

  TickIntervalStats* mutable_proc_time() { return &proc_time_; }

 private:
  TickIntervalStats proc_time_;
};

// Used to read from and write to WavFile objects.
class WavFileProcessor final : public AudioFileProcessor {
 public:
  // Takes ownership of all parameters.
  WavFileProcessor(std::unique_ptr<AudioProcessing> ap,
                   std::unique_ptr<WavReader> in_file,
                   std::unique_ptr<WavWriter> out_file,
                   std::unique_ptr<WavReader> reverse_in_file,
                   std::unique_ptr<WavWriter> reverse_out_file);
  virtual ~WavFileProcessor() {}

  // Processes one chunk from the WAV input and writes to the WAV output.
  bool ProcessChunk() override;

 private:
  std::unique_ptr<AudioProcessing> ap_;

  ChannelBuffer<float> in_buf_;
  ChannelBuffer<float> out_buf_;
  const StreamConfig input_config_;
  const StreamConfig output_config_;
  ChannelBufferWavReader buffer_reader_;
  ChannelBufferWavWriter buffer_writer_;
  std::unique_ptr<ChannelBuffer<float>> reverse_in_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_out_buf_;
  std::unique_ptr<StreamConfig> reverse_input_config_;
  std::unique_ptr<StreamConfig> reverse_output_config_;
  std::unique_ptr<ChannelBufferWavReader> reverse_buffer_reader_;
  std::unique_ptr<ChannelBufferWavWriter> reverse_buffer_writer_;
};

// Used to read from an aecdump file and write to a WavWriter.
class AecDumpFileProcessor final : public AudioFileProcessor {
 public:
  // Takes ownership of all parameters.
  AecDumpFileProcessor(std::unique_ptr<AudioProcessing> ap,
                       FILE* dump_file,
                       std::unique_ptr<WavWriter> out_file);

  virtual ~AecDumpFileProcessor();

  // Processes messages from the aecdump file until the first Stream message is
  // completed. Passes other data from the aecdump messages as appropriate.
  bool ProcessChunk() override;

 private:
  void HandleMessage(const webrtc::audioproc::Init& msg);
  void HandleMessage(const webrtc::audioproc::Stream& msg);
  void HandleMessage(const webrtc::audioproc::ReverseStream& msg);

  std::unique_ptr<AudioProcessing> ap_;
  FILE* dump_file_;

  std::unique_ptr<ChannelBuffer<float>> in_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_buf_;
  ChannelBuffer<float> out_buf_;
  StreamConfig input_config_;
  StreamConfig reverse_config_;
  const StreamConfig output_config_;
  ChannelBufferWavWriter buffer_writer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_AUDIO_FILE_PROCESSOR_H_
