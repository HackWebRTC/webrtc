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
 * This header file includes declaration for unit tests for the VP8 packetizer.
 */

#ifndef WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_
#define WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_

#include <gtest/gtest.h>

#include "typedefs.h"

namespace webrtc {

class RTPFragmentationHeader;

class RTPFormatVP8Test : public ::testing::Test {
 protected:
  RTPFormatVP8Test();
  virtual void SetUp();
  virtual void TearDown();
  WebRtc_UWord8* payload_data;
  WebRtc_UWord32 payload_size;
  RTPFragmentationHeader* fragmentation;
};

}

#endif  // WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_
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
 * This header file includes declaration for unit tests for the VP8 packetizer.
 */

#ifndef WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_
#define WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_

#include <gtest/gtest.h>

#include "typedefs.h"

namespace webrtc {

class RTPFragmentationHeader;

class RTPFormatVP8Test : public ::testing::Test {
 protected:
  RTPFormatVP8Test();
  virtual void SetUp();
  virtual void TearDown();
  WebRtc_UWord8* payload_data;
  WebRtc_UWord32 payload_size;
  RTPFragmentationHeader* fragmentation;
};

}

#endif  // WEBRTC_RTP_FORMAT_VP8_UNIT_TEST_H_
