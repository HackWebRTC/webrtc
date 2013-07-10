/*
 * libjingle
 * Copyright 2009 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/socketaddress.h"
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/stunport.h"
#include "talk/p2p/base/teststunserver.h"

using talk_base::SocketAddress;

static const SocketAddress kLocalAddr("127.0.0.1", 0);
static const SocketAddress kStunAddr("127.0.0.1", 5000);
static const SocketAddress kBadAddr("0.0.0.1", 5000);
static const SocketAddress kStunHostnameAddr("localhost", 5000);
static const SocketAddress kBadHostnameAddr("not-a-real-hostname", 5000);
static const int kTimeoutMs = 10000;

// Tests connecting a StunPort to a fake STUN server (cricket::StunServer)
// TODO: Use a VirtualSocketServer here. We have to use a
// PhysicalSocketServer right now since DNS is not part of SocketServer yet.
class StunPortTest : public testing::Test,
                     public sigslot::has_slots<> {
 public:
  StunPortTest()
      : network_("unittest", "unittest", talk_base::IPAddress(INADDR_ANY), 32),
        socket_factory_(talk_base::Thread::Current()),
        stun_server_(new cricket::TestStunServer(
          talk_base::Thread::Current(), kStunAddr)),
        done_(false), error_(false), stun_keepalive_delay_(0) {
  }

  const cricket::Port* port() const { return stun_port_.get(); }
  bool done() const { return done_; }
  bool error() const { return error_; }

  void CreateStunPort(const talk_base::SocketAddress& server_addr) {
    stun_port_.reset(cricket::StunPort::Create(
        talk_base::Thread::Current(), &socket_factory_, &network_,
        kLocalAddr.ipaddr(), 0, 0, talk_base::CreateRandomString(16),
        talk_base::CreateRandomString(22), server_addr));
    stun_port_->set_stun_keepalive_delay(stun_keepalive_delay_);
    stun_port_->SignalPortComplete.connect(this,
        &StunPortTest::OnPortComplete);
    stun_port_->SignalPortError.connect(this,
        &StunPortTest::OnPortError);
  }

  void PrepareAddress() {
    stun_port_->PrepareAddress();
  }

 protected:
  static void SetUpTestCase() {
    // Ensure the RNG is inited.
    talk_base::InitRandom(NULL, 0);
  }

  void OnPortComplete(cricket::Port* port) {
    done_ = true;
    error_ = false;
  }
  void OnPortError(cricket::Port* port) {
    done_ = true;
    error_ = true;
  }
  void SetKeepaliveDelay(int delay) {
    stun_keepalive_delay_ = delay;
  }

 private:
  talk_base::Network network_;
  talk_base::BasicPacketSocketFactory socket_factory_;
  talk_base::scoped_ptr<cricket::StunPort> stun_port_;
  talk_base::scoped_ptr<cricket::TestStunServer> stun_server_;
  bool done_;
  bool error_;
  int stun_keepalive_delay_;
};

// Test that we can create a STUN port
TEST_F(StunPortTest, TestBasic) {
  CreateStunPort(kStunAddr);
  EXPECT_EQ("stun", port()->Type());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can get an address from a STUN server.
TEST_F(StunPortTest, TestPrepareAddress) {
  CreateStunPort(kStunAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));

  // TODO: Add IPv6 tests here, once either physicalsocketserver supports
  // IPv6, or this test is changed to use VirtualSocketServer.
}

// Test that we fail properly if we can't get an address.
TEST_F(StunPortTest, TestPrepareAddressFail) {
  CreateStunPort(kBadAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can get an address from a STUN server specified by a hostname.
TEST_F(StunPortTest, TestPrepareAddressHostname) {
  CreateStunPort(kStunHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));
}

// Test that we handle hostname lookup failures properly.
TEST_F(StunPortTest, TestPrepareAddressHostnameFail) {
  CreateStunPort(kBadHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// This test verifies keepalive response messages don't result in
// additional candidate generation.
TEST_F(StunPortTest, TestKeepAliveResponse) {
  SetKeepaliveDelay(500);  // 500ms of keepalive delay.
  CreateStunPort(kStunHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));
  // Waiting for 1 seond, which will allow us to process
  // response for keepalive binding request. 500 ms is the keepalive delay.
  talk_base::Thread::Current()->ProcessMessages(1000);
  ASSERT_EQ(1U, port()->Candidates().size());
}

