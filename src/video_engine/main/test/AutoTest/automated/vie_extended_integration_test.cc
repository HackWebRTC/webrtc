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

class ViEExtendedIntegrationTest: public ViEIntegrationTest {
};

TEST_F(ViEExtendedIntegrationTest, RunsBaseTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEBaseExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsCaptureTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViECaptureExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsCodecTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViECodecExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsEncryptionTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEEncryptionExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsFileTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEFileExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsImageProcessTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViEImageProcessExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsNetworkTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViENetworkExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsRenderTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViERenderExtendedTest());
}

TEST_F(ViEExtendedIntegrationTest, RunsRtpRtcpTestWithoutErrors) {
  ASSERT_EQ(0, tests_->ViERtpRtcpExtendedTest());
}

} // namespace
