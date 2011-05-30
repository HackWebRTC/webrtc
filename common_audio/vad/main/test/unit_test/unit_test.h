/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This header file includes the declaration of the VAD unit test.
 */

#ifndef WEBRTC_VAD_UNIT_TEST_H_
#define WEBRTC_VAD_UNIT_TEST_H_

#include <gtest/gtest.h>

class VadTest : public ::testing::Test {
 protected:
  VadTest();
  virtual void SetUp();
  virtual void TearDown();
};

#endif  // WEBRTC_VAD_UNIT_TEST_H_
