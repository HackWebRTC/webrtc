/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Commandline tool to unpack audioproc debug files.
//
// The debug files are dumped as protobuf blobs. For analysis, it's necessary
// to unpack the file into its component parts: audio and other data.

#include <stdio.h>
#include <limits>

#include "gflags/gflags.h"
#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/wav_writer.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

// TODO(andrew): unpack more of the data.
DEFINE_string(input_file, "input.pcm", "The name of the input stream file.");
DEFINE_string(input_wav_file, "input.wav",
              "The name of the WAV input stream file.");
DEFINE_string(output_file, "ref_out.pcm",
              "The name of the reference output stream file.");
DEFINE_string(output_wav_file, "ref_out.wav",
              "The name of the WAV reference output stream file.");
DEFINE_string(reverse_file, "reverse.pcm",
              "The name of the reverse input stream file.");
DEFINE_string(reverse_wav_file, "reverse.wav",
              "The name of the WAV reverse input stream file.");
DEFINE_string(delay_file, "delay.int32", "The name of the delay file.");
DEFINE_string(drift_file, "drift.int32", "The name of the drift file.");
DEFINE_string(level_file, "level.int32", "The name of the level file.");
DEFINE_string(keypress_file, "keypress.bool", "The name of the keypress file.");
DEFINE_string(settings_file, "settings.txt", "The name of the settings file.");
DEFINE_bool(full, false,
            "Unpack the full set of files (normally not needed).");
DEFINE_bool(pcm, false, "Write to PCM instead of WAV file.");

namespace webrtc {

using audioproc::Event;
using audioproc::ReverseStream;
using audioproc::Stream;
using audioproc::Init;

class PcmFile {
 public:
  PcmFile(const std::string& filename)
      : file_handle_(fopen(filename.c_str(), "wb")) {}

  ~PcmFile() {
    fclose(file_handle_);
  }

  void WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to PCM file"
#endif
    fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  }

  void WriteSamples(const float* samples, size_t num_samples) {
    static const size_t kChunksize = 4096 / sizeof(uint16_t);
    for (size_t i = 0; i < num_samples; i += kChunksize) {
      int16_t isamples[kChunksize];
      const size_t chunk = std::min(kChunksize, num_samples - i);
      RoundToInt16(samples + i, chunk, isamples);
      WriteSamples(isamples, chunk);
    }
  }

 private:
  FILE* file_handle_;
};

void WriteData(const void* data, size_t size, FILE* file,
               const std::string& filename) {
  if (fwrite(data, size, 1, file) != 1) {
    printf("Error when writing to %s\n", filename.c_str());
    exit(1);
  }
}

void WriteIntData(const int16_t* data,
                  size_t length,
                  WavFile* wav_file,
                  PcmFile* pcm_file) {
  if (wav_file) {
    wav_file->WriteSamples(data, length);
  }
  if (pcm_file) {
    pcm_file->WriteSamples(data, length);
  }
}

void WriteFloatData(const float* const* data,
                    size_t samples_per_channel,
                    int num_channels,
                    WavFile* wav_file,
                    PcmFile* pcm_file) {
  size_t length = num_channels * samples_per_channel;
  scoped_ptr<float[]> buffer(new float[length]);
  Interleave(data, samples_per_channel, num_channels, buffer.get());
  // TODO(aluebs): Use ScaleToInt16Range() from audio_util
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = buffer[i] > 0 ?
                buffer[i] * std::numeric_limits<int16_t>::max() :
                -buffer[i] * std::numeric_limits<int16_t>::min();
  }
  if (wav_file) {
    wav_file->WriteSamples(buffer.get(), length);
  }
  if (pcm_file) {
    pcm_file->WriteSamples(buffer.get(), length);
  }
}

int do_main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage = "Commandline tool to unpack audioproc debug files.\n"
    "Example usage:\n" + program_name + " debug_dump.pb\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 2) {
    printf("%s", google::ProgramUsage());
    return 1;
  }

  FILE* debug_file = OpenFile(argv[1], "rb");

  Event event_msg;
  int frame_count = 0;
  int num_input_channels = 0;
  int num_output_channels = 0;
  int num_reverse_channels = 0;
  scoped_ptr<WavFile> reverse_wav_file;
  scoped_ptr<WavFile> input_wav_file;
  scoped_ptr<WavFile> output_wav_file;
  scoped_ptr<PcmFile> reverse_pcm_file;
  scoped_ptr<PcmFile> input_pcm_file;
  scoped_ptr<PcmFile> output_pcm_file;
  while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupt input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (msg.has_data()) {
        WriteIntData(reinterpret_cast<const int16_t*>(msg.data().data()),
                     msg.data().size() / sizeof(int16_t),
                     reverse_wav_file.get(),
                     reverse_pcm_file.get());
      } else if (msg.channel_size() > 0) {
        scoped_ptr<const float*[]> data(new const float*[num_reverse_channels]);
        for (int i = 0; i < num_reverse_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.channel(i).data());
        }
        WriteFloatData(data.get(),
                       msg.channel(0).size() / sizeof(float),
                       num_reverse_channels,
                       reverse_wav_file.get(),
                       reverse_pcm_file.get());
      }
    } else if (event_msg.type() == Event::STREAM) {
      frame_count++;
      if (!event_msg.has_stream()) {
        printf("Corrupt input file: Stream missing.\n");
        return 1;
      }

      const Stream msg = event_msg.stream();
      if (msg.has_input_data()) {
        WriteIntData(reinterpret_cast<const int16_t*>(msg.input_data().data()),
                     msg.input_data().size() / sizeof(int16_t),
                     input_wav_file.get(),
                     input_pcm_file.get());
      } else if (msg.input_channel_size() > 0) {
        scoped_ptr<const float*[]> data(new const float*[num_input_channels]);
        for (int i = 0; i < num_input_channels; ++i) {
          data[i] = reinterpret_cast<const float*>(msg.input_channel(i).data());
        }
        WriteFloatData(data.get(),
                       msg.input_channel(0).size() / sizeof(float),
                       num_input_channels,
                       input_wav_file.get(),
                       input_pcm_file.get());
      }

      if (msg.has_output_data()) {
        WriteIntData(reinterpret_cast<const int16_t*>(msg.output_data().data()),
                     msg.output_data().size() / sizeof(int16_t),
                     output_wav_file.get(),
                     output_pcm_file.get());
      } else if (msg.output_channel_size() > 0) {
        scoped_ptr<const float*[]> data(new const float*[num_output_channels]);
        for (int i = 0; i < num_output_channels; ++i) {
          data[i] =
              reinterpret_cast<const float*>(msg.output_channel(i).data());
        }
        WriteFloatData(data.get(),
                       msg.output_channel(0).size() / sizeof(float),
                       num_output_channels,
                       output_wav_file.get(),
                       output_pcm_file.get());
      }

      if (FLAGS_full) {
        if (msg.has_delay()) {
          static FILE* delay_file = OpenFile(FLAGS_delay_file, "wb");
          int32_t delay = msg.delay();
          WriteData(&delay, sizeof(delay), delay_file, FLAGS_delay_file);
        }

        if (msg.has_drift()) {
          static FILE* drift_file = OpenFile(FLAGS_drift_file, "wb");
          int32_t drift = msg.drift();
          WriteData(&drift, sizeof(drift), drift_file, FLAGS_drift_file);
        }

        if (msg.has_level()) {
          static FILE* level_file = OpenFile(FLAGS_level_file, "wb");
          int32_t level = msg.level();
          WriteData(&level, sizeof(level), level_file, FLAGS_level_file);
        }

        if (msg.has_keypress()) {
          static FILE* keypress_file = OpenFile(FLAGS_keypress_file, "wb");
          bool keypress = msg.keypress();
          WriteData(&keypress, sizeof(keypress), keypress_file,
                    FLAGS_keypress_file);
        }
      }
    } else if (event_msg.type() == Event::INIT) {
      if (!event_msg.has_init()) {
        printf("Corrupt input file: Init missing.\n");
        return 1;
      }

      static FILE* settings_file = OpenFile(FLAGS_settings_file, "wb");
      const Init msg = event_msg.init();
      // These should print out zeros if they're missing.
      fprintf(settings_file, "Init at frame: %d\n", frame_count);
      int input_sample_rate = msg.sample_rate();
      fprintf(settings_file, "  Input sample rate: %d\n", input_sample_rate);
      int output_sample_rate = msg.output_sample_rate();
      fprintf(settings_file, "  Output sample rate: %d\n", output_sample_rate);
      int reverse_sample_rate = msg.reverse_sample_rate();
      fprintf(settings_file,
              "  Reverse sample rate: %d\n",
              reverse_sample_rate);
      num_input_channels = msg.num_input_channels();
      fprintf(settings_file, "  Input channels: %d\n", num_input_channels);
      num_output_channels = msg.num_output_channels();
      fprintf(settings_file, "  Output channels: %d\n", num_output_channels);
      num_reverse_channels = msg.num_reverse_channels();
      fprintf(settings_file, "  Reverse channels: %d\n", num_reverse_channels);

      fprintf(settings_file, "\n");

      if (reverse_sample_rate == 0) {
        reverse_sample_rate = input_sample_rate;
      }
      if (output_sample_rate == 0) {
        output_sample_rate = input_sample_rate;
      }

      if (FLAGS_pcm) {
        if (!reverse_pcm_file.get()) {
          reverse_pcm_file.reset(new PcmFile(FLAGS_reverse_file));
        }
        if (!input_pcm_file.get()) {
          input_pcm_file.reset(new PcmFile(FLAGS_input_file));
        }
        if (!output_pcm_file.get()) {
          output_pcm_file.reset(new PcmFile(FLAGS_output_file));
        }
      } else {
        reverse_wav_file.reset(new WavFile(FLAGS_reverse_wav_file,
                                       reverse_sample_rate,
                                       num_reverse_channels));
        input_wav_file.reset(new WavFile(FLAGS_input_wav_file,
                                     input_sample_rate,
                                     num_input_channels));
        output_wav_file.reset(new WavFile(FLAGS_output_wav_file,
                                      output_sample_rate,
                                      num_output_channels));
      }
    }
  }

  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::do_main(argc, argv);
}
