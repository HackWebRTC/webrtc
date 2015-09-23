/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/session.h"
#include "webrtc/p2p/base/transportchannelproxy.h"
#include "webrtc/p2p/client/fakeportallocator.h"

using cricket::BaseSession;
using cricket::DtlsTransportChannelWrapper;
using cricket::FakePortAllocator;
using cricket::P2PTransportChannel;
using cricket::PortAllocator;
using cricket::TransportChannelProxy;
using cricket::TransportProxy;

class BaseSessionForTest : public BaseSession {
 public:
  BaseSessionForTest(rtc::Thread* signaling_thread,
                     rtc::Thread* worker_thread,
                     PortAllocator* port_allocator,
                     const std::string& sid,
                     const std::string& content_type,
                     bool initiator)
      : BaseSession(signaling_thread,
                    worker_thread,
                    port_allocator,
                    sid,
                    content_type,
                    initiator) {}
  using BaseSession::GetOrCreateTransportProxy;
};

class BaseSessionTest : public testing::Test {
 public:
  BaseSessionTest()
      : port_allocator_(new FakePortAllocator(rtc::Thread::Current(), nullptr)),
        session_(new BaseSessionForTest(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        port_allocator_.get(),
                                        "123",
                                        cricket::NS_JINGLE_RTP,
                                        false)) {}
  P2PTransportChannel* CreateChannel(const std::string& content,
                                     int component) {
    TransportProxy* transport_proxy =
        session_->GetOrCreateTransportProxy(content);
    // This hacking is needed in order that the p2p transport channel
    // will be created in the following.
    transport_proxy->CompleteNegotiation();

    TransportChannelProxy* channel_proxy = static_cast<TransportChannelProxy*>(
        session_->CreateChannel(content, component));
    DtlsTransportChannelWrapper* dtls_channel =
        static_cast<DtlsTransportChannelWrapper*>(channel_proxy->impl());
    return static_cast<P2PTransportChannel*>(dtls_channel->channel());
  }

  rtc::scoped_ptr<PortAllocator> port_allocator_;
  rtc::scoped_ptr<BaseSessionForTest> session_;
};

TEST_F(BaseSessionTest, TestSetIceReceivingTimeout) {
  P2PTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(channel1, nullptr);
  // These are the default values.
  EXPECT_EQ(2500, channel1->receiving_timeout());
  EXPECT_EQ(250, channel1->check_receiving_delay());
  // Set the timeout to a different value.
  session_->SetIceConnectionReceivingTimeout(1000);
  EXPECT_EQ(1000, channel1->receiving_timeout());
  EXPECT_EQ(100, channel1->check_receiving_delay());

  // Even if a channel is created after setting the receiving timeout,
  // the set timeout value is applied to the new channel.
  P2PTransportChannel* channel2 = CreateChannel("video", 2);
  ASSERT_NE(channel2, nullptr);
  EXPECT_EQ(1000, channel2->receiving_timeout());
  EXPECT_EQ(100, channel2->check_receiving_delay());

  // Test minimum checking delay.
  session_->SetIceConnectionReceivingTimeout(200);
  EXPECT_EQ(200, channel1->receiving_timeout());
  EXPECT_EQ(50, channel1->check_receiving_delay());
  EXPECT_EQ(200, channel2->receiving_timeout());
  EXPECT_EQ(50, channel2->check_receiving_delay());
}
