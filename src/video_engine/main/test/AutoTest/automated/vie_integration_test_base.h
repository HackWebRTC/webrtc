/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_VIE_INTEGRATION_TEST_BASE_H_
#define SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_VIE_INTEGRATION_TEST_BASE_H_

#include "gtest/gtest.h"

class ViEWindowCreator;
class ViEAutoTest;

// Meant to be interited by standard integration tests based on
// ViEAutoTest.
class ViEIntegrationTest: public testing::Test {
 public:
  // Intitializes a suitable webcam on the system and launches
  // two windows in a platform-dependent manner.
  static void SetUpTestCase();

  // Releases anything allocated by SetupTestCase.
  static void TearDownTestCase();

 protected:
  static ViEWindowCreator* window_creator_;
  static ViEAutoTest* tests_;
};

#endif  // SRC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_AUTOMATED_VIE_INTEGRATION_TEST_BASE_H_
