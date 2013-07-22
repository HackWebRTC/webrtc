/*
 * libjingle
 * Copyright 2012 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vector>

#include "talk/base/fakenetwork.h"
#include "talk/base/gunit.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/portallocatorsessionproxy.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/p2p/client/fakeportallocator.h"

using cricket::Candidate;
using cricket::PortAllocatorSession;
using cricket::PortAllocatorSessionMuxer;
using cricket::PortAllocatorSessionProxy;

// Based on ICE_UFRAG_LENGTH
static const char kIceUfrag0[] = "TESTICEUFRAG0000";
// Based on ICE_PWD_LENGTH
static const char kIcePwd0[] = "TESTICEPWD00000000000000";

class TestSessionChannel : public sigslot::has_slots<> {
 public:
  explicit TestSessionChannel(PortAllocatorSessionProxy* proxy)
      : proxy_session_(proxy),
        candidates_count_(0),
        allocation_complete_(false),
        ports_count_(0) {
    proxy_session_->SignalCandidatesAllocationDone.connect(
        this, &TestSessionChannel::OnCandidatesAllocationDone);
    proxy_session_->SignalCandidatesReady.connect(
        this, &TestSessionChannel::OnCandidatesReady);
    proxy_session_->SignalPortReady.connect(
        this, &TestSessionChannel::OnPortReady);
  }
  virtual ~TestSessionChannel() {}
  void OnCandidatesReady(PortAllocatorSession* session,
                         const std::vector<Candidate>& candidates) {
    EXPECT_EQ(proxy_session_, session);
    candidates_count_ += static_cast<int>(candidates.size());
  }
  void OnCandidatesAllocationDone(PortAllocatorSession* session) {
    EXPECT_EQ(proxy_session_, session);
    allocation_complete_ = true;
  }
  void OnPortReady(PortAllocatorSession* session,
                   cricket::PortInterface* port) {
    EXPECT_EQ(proxy_session_, session);
    ++ports_count_;
  }
  int candidates_count() { return candidates_count_; }
  bool allocation_complete() { return allocation_complete_; }
  int ports_count() { return ports_count_; }

  void StartGettingPorts() {
    proxy_session_->StartGettingPorts();
  }

  void StopGettingPorts() {
    proxy_session_->StopGettingPorts();
  }

  bool IsGettingPorts() {
    return proxy_session_->IsGettingPorts();
  }

 private:
  PortAllocatorSessionProxy* proxy_session_;
  int candidates_count_;
  bool allocation_complete_;
  int ports_count_;
};

class PortAllocatorSessionProxyTest : public testing::Test {
 public:
  PortAllocatorSessionProxyTest()
      : socket_factory_(talk_base::Thread::Current()),
        allocator_(talk_base::Thread::Current(), NULL),
        session_(talk_base::Thread::Current(), &socket_factory_,
                 "test content", 1,
                 kIceUfrag0, kIcePwd0),
        session_muxer_(new PortAllocatorSessionMuxer(&session_)) {
  }
  virtual ~PortAllocatorSessionProxyTest() {}
  void RegisterSessionProxy(PortAllocatorSessionProxy* proxy) {
    session_muxer_->RegisterSessionProxy(proxy);
  }

  TestSessionChannel* CreateChannel() {
    PortAllocatorSessionProxy* proxy =
        new PortAllocatorSessionProxy("test content", 1, 0);
    TestSessionChannel* channel = new TestSessionChannel(proxy);
    session_muxer_->RegisterSessionProxy(proxy);
    channel->StartGettingPorts();
    return channel;
  }

 protected:
  talk_base::BasicPacketSocketFactory socket_factory_;
  cricket::FakePortAllocator allocator_;
  cricket::FakePortAllocatorSession session_;
  // Muxer object will be delete itself after all registered session proxies
  // are deleted.
  PortAllocatorSessionMuxer* session_muxer_;
};

TEST_F(PortAllocatorSessionProxyTest, TestBasic) {
  TestSessionChannel* channel = CreateChannel();
  EXPECT_EQ_WAIT(1, channel->candidates_count(), 1000);
  EXPECT_EQ(1, channel->ports_count());
  EXPECT_TRUE(channel->allocation_complete());
  delete channel;
}

TEST_F(PortAllocatorSessionProxyTest, TestLateBinding) {
  TestSessionChannel* channel1 = CreateChannel();
  EXPECT_EQ_WAIT(1, channel1->candidates_count(), 1000);
  EXPECT_EQ(1, channel1->ports_count());
  EXPECT_TRUE(channel1->allocation_complete());
  EXPECT_EQ(1, session_.port_config_count());
  // Creating another PortAllocatorSessionProxy and it also should receive
  // already happened events.
  PortAllocatorSessionProxy* proxy =
      new PortAllocatorSessionProxy("test content", 2, 0);
  TestSessionChannel* channel2 = new TestSessionChannel(proxy);
  session_muxer_->RegisterSessionProxy(proxy);
  EXPECT_TRUE(channel2->IsGettingPorts());
  EXPECT_EQ_WAIT(1, channel2->candidates_count(), 1000);
  EXPECT_EQ(1, channel2->ports_count());
  EXPECT_TRUE_WAIT(channel2->allocation_complete(), 1000);
  EXPECT_EQ(1, session_.port_config_count());
  delete channel1;
  delete channel2;
}
