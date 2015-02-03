/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/session.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cricket {

class BaseSessionTest : public testing::Test, public BaseSession {
 protected:
  BaseSessionTest() :
      BaseSession(rtc::Thread::Current(), rtc::Thread::Current(),
                  NULL, "sid", "video?", true) {
  }

  ~BaseSessionTest() {}
};

// Tests that channels are not being deleted until all refcounts are
// used and that the TransportProxy is not removed unless all channels
// are removed from the proxy.
TEST_F(BaseSessionTest, TransportChannelProxyRefCounter) {
  std::string content_name = "no matter";
  int component = 10;
  TransportChannel* channel = CreateChannel(content_name, "", component);
  TransportChannel* channel_again = CreateChannel(content_name, "", component);

  EXPECT_EQ(channel, channel_again);
  EXPECT_EQ(channel, GetChannel(content_name, component));

  DestroyChannel(content_name, component);
  EXPECT_EQ(channel, GetChannel(content_name, component));

  // Try to destroy a non-existant content name.
  DestroyTransportProxyWhenUnused("other content");
  EXPECT_TRUE(GetTransportProxy(content_name) != NULL);

  DestroyChannel(content_name, component);
  EXPECT_EQ(NULL, GetChannel(content_name, component));
  EXPECT_TRUE(GetTransportProxy(content_name) != NULL);

  DestroyTransportProxyWhenUnused(content_name);
  EXPECT_TRUE(GetTransportProxy(content_name) == NULL);
}

}  // namespace cricket
