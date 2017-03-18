/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>

#include "gflags/gflags.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/config.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

// Adapting DirExists/FileExists interfaces to DEFINE_validator.
auto dir_exists = [](const char* c, const std::string& dirpath) {
  return DirExists(dirpath);
};
auto file_exists = [](const char* c, const std::string& filepath) {
  return FileExists(filepath);
};

const char kUsageDescription[] =
    "Usage: conversational_speech_generator\n"
    "          -i <path/to/source/audiotracks>\n"
    "          -t <path/to/timing_file.txt>\n"
    "          -o <output/path>\n"
    "\n\n"
    "Command-line tool to generate multiple-end audio tracks to simulate "
    "conversational speech with two or more participants.";

DEFINE_string(i, "", "Directory containing the speech turn wav files");
DEFINE_validator(i, dir_exists);
DEFINE_string(t, "", "Path to the timing text file");
DEFINE_validator(t, file_exists);
DEFINE_string(o, "", "Output wav files destination path");
DEFINE_validator(o, dir_exists);

}  // namespace

int main(int argc, char* argv[]) {
  google::SetUsageMessage(kUsageDescription);
  google::ParseCommandLineFlags(&argc, &argv, true);

  conversational_speech::Config config(FLAGS_i, FLAGS_t, FLAGS_o);

  // TODO(alessiob): remove line below once debugged.
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
  LOG(LS_VERBOSE) << "i = " << config.audiotracks_path();
  LOG(LS_VERBOSE) << "t = " << config.timing_filepath();
  LOG(LS_VERBOSE) << "o = " << config.output_path();

  return 0;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
