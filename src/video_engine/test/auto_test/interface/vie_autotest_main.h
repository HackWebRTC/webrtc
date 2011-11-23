/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_

#include <string>
#include <map>

class ViEAutoTestMain {
 public:
  ViEAutoTestMain();
  bool BeginOSIndependentTesting();
  bool GetAnswer(int index, std::string* answer);
  int AskUserForTestCase();
  bool GetNextAnswer(std::string& answer);
  bool IsUsingAnswerFile();
  bool UseAnswerFile(const char* fileName);

 private:
  std::string answers_[1024];
  int         answers_count_;
  int         answers_index_;
  bool        use_answer_file_;
  std::map<int, std::string> index_to_test_method_map_;

  static const int kInvalidChoice = -1;

  // Prompts the user for a specific test method in the provided test case.
  // Returns 0 on success, nonzero otherwise.
  int RunSpecificTestCaseIn(const std::string test_case_name);
  // Retrieves a number from the user in the interval
  // [min_allowed, max_allowed]. Returns kInvalidChoice on failure.
  int AskUserForNumber(int min_allowed, int max_allowed);
  // Runs all tests matching the provided filter. * are wildcards.
  // Returns the test runner result (0 == OK).
  int RunTestMatching(const std::string test_case,
                      const std::string test_method);
  // Runs a non-gtest test case. Choice must be [7,9]. Returns 0 on success.
  int RunSpecialTestCase(int choice);
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_
