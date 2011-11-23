/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_integration_test_base.h"

#include "vie_autotest.h"
#include "vie_autotest_window_manager_interface.h"
#include "vie_window_creator.h"

void ViEIntegrationTest::SetUpTestCase() {
  window_creator_ = new ViEWindowCreator();

  ViEAutoTestWindowManagerInterface* window_manager =
      window_creator_->CreateTwoWindows();

  // Create the test cases
  tests_ = new ViEAutoTest(window_manager->GetWindow1(),
                           window_manager->GetWindow2());
}

void ViEIntegrationTest::TearDownTestCase() {
  window_creator_->TerminateWindows();

  delete tests_;
  delete window_creator_;
}

ViEWindowCreator* ViEIntegrationTest::window_creator_ = NULL;
ViEAutoTest* ViEIntegrationTest::tests_ = NULL;
