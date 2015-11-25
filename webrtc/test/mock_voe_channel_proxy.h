/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
#define WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_

#include <string>
#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/voice_engine/channel_proxy.h"

namespace webrtc {
namespace test {

class MockVoEChannelProxy : public voe::ChannelProxy {
 public:
  MOCK_METHOD1(SetRTCPStatus, void(bool enable));
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD1(SetRTCP_CNAME, void(const std::string& c_name));
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
