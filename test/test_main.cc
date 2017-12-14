/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics_default.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/perf_test.h"

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"

DEFINE_string(NSTreatUnknownArgumentsAsOpen, "",
    "Intentionally ignored flag intended for iOS simulator.");
DEFINE_string(ApplePersistenceIgnoreState, "",
    "Intentionally ignored flag intended for iOS simulator.");
#endif

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(force_fieldtrials, "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");

DEFINE_string(
    perf_results_json_path,
    "",
    "Path where the perf results should be stored it the JSON format described "
    "by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

DEFINE_bool(help, false, "Print this message.");

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);

  // Default to LS_INFO, even for release builds to provide better test logging.
  // TODO(pbos): Consider adding a command-line override.
  if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);

  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, false)) {
    return 1;
  }
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  webrtc::test::SetExecutablePath(argv[0]);
  std::string fieldtrials = FLAG_force_fieldtrials;
  webrtc::test::InitFieldTrialsFromString(fieldtrials);
  webrtc::metrics::Enable();

  rtc::LogMessage::SetLogToStderr(FLAG_logs);
#if defined(WEBRTC_IOS)
  rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv);
  rtc::test::RunTestsFromIOSApp();
#endif

  int exit_code = RUN_ALL_TESTS();

  std::string perf_results_json_path = FLAG_perf_results_json_path;
  if (perf_results_json_path != "") {
    std::string json_results = webrtc::test::GetPerfResultsJSON();
    rtc::File json_file = rtc::File::Open(perf_results_json_path);
    json_file.Write(reinterpret_cast<const uint8_t*>(json_results.c_str()),
                    json_results.size());
    json_file.Close();
  }

  return exit_code;
}
