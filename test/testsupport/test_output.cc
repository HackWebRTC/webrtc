/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/test_output.h"

#include <string.h>

#include "webrtc/rtc_base/file.h"
#include "webrtc/rtc_base/flags.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/pathutils.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace {
const std::string& DefaultOutputPath() {
  static const std::string path = webrtc::test::OutputPath();
  return path;
}
}

DEFINE_string(test_output_dir,
              DefaultOutputPath().c_str(),
              "The output folder where test output should be saved.");

namespace webrtc {
namespace test {

bool GetTestOutputDir(std::string* out_dir) {
  if (strlen(FLAG_test_output_dir) == 0) {
    LOG(LS_WARNING) << "No test_out_dir defined.";
    return false;
  }
  *out_dir = FLAG_test_output_dir;
  return true;
}

bool WriteToTestOutput(const char* filename,
                       const uint8_t* buffer,
                       size_t length) {
  if (strlen(FLAG_test_output_dir) == 0) {
    LOG(LS_WARNING) << "No test_out_dir defined.";
    return false;
  }

  if (filename == nullptr || strlen(filename) == 0) {
    LOG(LS_WARNING) << "filename must be provided.";
    return false;
  }

  rtc::File output =
      rtc::File::Create(rtc::Pathname(FLAG_test_output_dir, filename));

  return output.IsOpen() && output.Write(buffer, length) == length;
}

bool WriteToTestOutput(const char* filename, const std::string& content) {
  return WriteToTestOutput(filename,
                           reinterpret_cast<const uint8_t*>(content.c_str()),
                           content.length());
}

}  // namespace test
}  // namespace webrtc
