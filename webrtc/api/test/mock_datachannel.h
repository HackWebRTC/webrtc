/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_TEST_MOCK_DATACHANNEL_H_
#define WEBRTC_API_TEST_MOCK_DATACHANNEL_H_

#include "webrtc/api/datachannel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webrtc {

class MockDataChannel : public rtc::RefCountedObject<DataChannel> {
 public:
  explicit MockDataChannel(DataState state)
      : rtc::RefCountedObject<DataChannel>(
            nullptr, cricket::DCT_NONE, "MockDataChannel") {
    EXPECT_CALL(*this, state()).WillRepeatedly(testing::Return(state));
  }
  MOCK_CONST_METHOD0(state, DataState());
};

}  // namespace webrtc

#endif  // WEBRTC_API_TEST_MOCK_DATACHANNEL_H_
