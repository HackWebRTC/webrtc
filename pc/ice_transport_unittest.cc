/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/ice_transport.h"
#include "p2p/base/fake_port_allocator.h"

#include "test/gtest.h"

namespace webrtc {

class IceTransportTest : public testing::Test {};

TEST_F(IceTransportTest, CreateStandaloneIceTransport) {
  auto port_allocator = new cricket::FakePortAllocator(nullptr, nullptr);
  auto transport = CreateIceTransport(port_allocator);
  ASSERT_TRUE(transport->internal());
}

}  // namespace webrtc
