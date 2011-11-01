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

#include "google/gflags.h"
#include "scoped_ptr.h"
#include "typedefs.h"
#include "webrtc/audio_processing/debug.pb.h"

using webrtc::scoped_array;

using webrtc::audioproc::Event;
using webrtc::audioproc::ReverseStream;
using webrtc::audioproc::Stream;

// TODO(andrew): unpack more of the data.
DEFINE_string(input_file, "input.pcm", "The name of the input stream file.");
DEFINE_string(output_file, "output.pcm", "The name of the output stream file.");
DEFINE_string(reverse_file, "reverse.pcm", "The name of the reverse input "
              "file.");

// TODO(andrew): move this to a helper class to share with process_test.cc?
// Returns true on success, false on error or end-of-file.
bool ReadMessageFromFile(FILE* file,
                        ::google::protobuf::MessageLite* msg) {
  // The "wire format" for the size is little-endian.
  // Assume process_test is running on a little-endian machine.
  int32_t size = 0;
  if (fread(&size, sizeof(int32_t), 1, file) != 1) {
    return false;
  }
  if (size <= 0) {
    return false;
  }
  const size_t usize = static_cast<size_t>(size);

  scoped_array<char> array(new char[usize]);
  if (fread(array.get(), sizeof(char), usize, file) != usize) {
    return false;
  }

  msg->Clear();
  return msg->ParseFromArray(array.get(), usize);
}

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage = "Commandline tool to unpack audioproc debug files.\n"
    "Example usage:\n" + program_name + " debug_dump.pb\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 2) {
    printf("%s", google::ProgramUsage());
    return 1;
  }

  FILE* debug_file = fopen(argv[1], "rb");
  FILE* input_file = fopen(FLAGS_input_file.c_str(), "wb");
  FILE* output_file = fopen(FLAGS_output_file.c_str(), "wb");
  FILE* reverse_file = fopen(FLAGS_reverse_file.c_str(), "wb");
  Event event_msg;
  while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupted input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (!msg.has_data()) {
        printf("Corrupted input file: ReverseStream::data missing.\n");
        return 1;
      }
      if (fwrite(msg.data().data(), msg.data().size(), 1, reverse_file) != 1) {
        printf("Error when writing to %s\n", FLAGS_reverse_file.c_str());
        return 1;
      }
    } else if (event_msg.type() == Event::STREAM) {
      if (!event_msg.has_stream()) {
        printf("Corrupted input file: Stream missing.\n");
        return 1;
      }

      const Stream msg = event_msg.stream();
      if (!msg.has_input_data()) {
        printf("Corrupted input file: Stream::input_data missing.\n");
        return 1;
      }
      if (fwrite(msg.input_data().data(), msg.input_data().size(), 1,
                 input_file) != 1) {
        printf("Error when writing to %s\n", FLAGS_input_file.c_str());
        return 1;
      }

      if (!msg.has_output_data()) {
        printf("Corrupted input file: Stream::output_data missing.\n");
        return 1;
      }
      if (fwrite(msg.output_data().data(), msg.output_data().size(), 1,
                 output_file) != 1) {
        printf("Error when writing to %s\n", FLAGS_output_file.c_str());
        return 1;
      }
    }
  }

  return 0;
}
