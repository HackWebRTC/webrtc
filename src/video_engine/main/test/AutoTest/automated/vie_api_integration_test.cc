/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * Runs "extended" integration tests.
 */

#include "gtest/gtest.h"

#include "vie_integration_test_base.h"
#include "vie_autotest.h"

namespace {

class ViEApiIntegrationTest: public ViEIntegrationTest {
};

TEST_F(ViEApiIntegrationTest, RunsBaseTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEBaseAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsCaptureTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViECaptureAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsCodecTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViECodecAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsEncryptionTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEEncryptionAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsFileTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEFileAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsImageProcessTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEImageProcessAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsNetworkTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViENetworkAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsRenderTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViERenderAPITest());
}

TEST_F(ViEApiIntegrationTest, RunsRtpRtcpTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViERtpRtcpAPITest());
}
}
