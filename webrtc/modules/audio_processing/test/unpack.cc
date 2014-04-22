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

#include "gflags/gflags.h"
#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

// TODO(andrew): unpack more of the data.
DEFINE_string(input_file, "input.pcm", "The name of the input stream file.");
DEFINE_string(float_input_file, "input.float",
              "The name of the float input stream file.");
DEFINE_string(output_file, "ref_out.pcm",
              "The name of the reference output stream file.");
DEFINE_string(float_output_file, "ref_out.float",
              "The name of the float reference output stream file.");
DEFINE_string(reverse_file, "reverse.pcm",
              "The name of the reverse input stream file.");
DEFINE_string(float_reverse_file, "reverse.float",
              "The name of the float reverse input stream file.");
DEFINE_string(delay_file, "delay.int32", "The name of the delay file.");
DEFINE_string(drift_file, "drift.int32", "The name of the drift file.");
DEFINE_string(level_file, "level.int32", "The name of the level file.");
DEFINE_string(keypress_file, "keypress.bool", "The name of the keypress file.");
DEFINE_string(settings_file, "settings.txt", "The name of the settings file.");
DEFINE_bool(full, false,
            "Unpack the full set of files (normally not needed).");

namespace webrtc {

using audioproc::Event;
using audioproc::ReverseStream;
using audioproc::Stream;
using audioproc::Init;

void WriteData(const void* data, size_t size, FILE* file,
               const std::string& filename) {
  if (fwrite(data, size, 1, file) != 1) {
    printf("Error when writing to %s\n", filename.c_str());
    exit(1);
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
while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupt input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (msg.has_data()) {
        static FILE* reverse_file = OpenFile(FLAGS_reverse_file, "wb");
        WriteData(msg.data().data(), msg.data().size(), reverse_file,
                  FLAGS_reverse_file);

      } else if (msg.channel_size() > 0) {
        static FILE* float_reverse_file = OpenFile(FLAGS_float_reverse_file,
                                                   "wb");
        // TODO(ajm): Interleave multiple channels.
        assert(msg.channel_size() == 1);
        WriteData(msg.channel(0).data(), msg.channel(0).size(),
                  float_reverse_file, FLAGS_reverse_file);
      }
    } else if (event_msg.type() == Event::STREAM) {
      frame_count++;
      if (!event_msg.has_stream()) {
        printf("Corrupt input file: Stream missing.\n");
        return 1;
      }

      const Stream msg = event_msg.stream();
      if (msg.has_input_data()) {
        static FILE* input_file = OpenFile(FLAGS_input_file, "wb");
        WriteData(msg.input_data().data(), msg.input_data().size(),
                  input_file, FLAGS_input_file);

      } else if (msg.input_channel_size() > 0) {
        static FILE* float_input_file = OpenFile(FLAGS_float_input_file, "wb");
        // TODO(ajm): Interleave multiple channels.
        assert(msg.input_channel_size() == 1);
        WriteData(msg.input_channel(0).data(), msg.input_channel(0).size(),
                  float_input_file, FLAGS_float_input_file);
      }

      if (msg.has_output_data()) {
        static FILE* output_file = OpenFile(FLAGS_output_file, "wb");
        WriteData(msg.output_data().data(), msg.output_data().size(),
                  output_file, FLAGS_output_file);

      } else if (msg.output_channel_size() > 0) {
        static FILE* float_output_file = OpenFile(FLAGS_float_output_file,
                                                  "wb");
        // TODO(ajm): Interleave multiple channels.
        assert(msg.output_channel_size() == 1);
        WriteData(msg.output_channel(0).data(), msg.output_channel(0).size(),
                  float_output_file, FLAGS_float_output_file);
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
      fprintf(settings_file, "  Sample rate: %d\n", msg.sample_rate());
      fprintf(settings_file, "  Input channels: %d\n",
              msg.num_input_channels());
      fprintf(settings_file, "  Output channels: %d\n",
              msg.num_output_channels());
      fprintf(settings_file, "  Reverse channels: %d\n",
              msg.num_reverse_channels());

      fprintf(settings_file, "\n");
    }
  }

  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::do_main(argc, argv);
}
