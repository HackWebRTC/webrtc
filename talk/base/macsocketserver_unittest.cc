/*
 * libjingle
 * Copyright 2009, Google Inc.
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
#include "talk/base/scoped_ptr.h"
#include "talk/base/socket_unittest.h"
#include "talk/base/thread.h"
#include "talk/base/macsocketserver.h"

namespace talk_base {

class WakeThread : public Thread {
 public:
  WakeThread(SocketServer* ss) : ss_(ss) {
  }
  void Run() {
    ss_->WakeUp();
  }
 private:
  SocketServer* ss_;
};

#ifndef CARBON_DEPRECATED

// Test that MacCFSocketServer::Wait works as expected.
TEST(MacCFSocketServerTest, TestWait) {
  MacCFSocketServer server;
  uint32 start = Time();
  server.Wait(1000, true);
  EXPECT_GE(TimeSince(start), 1000);
}

// Test that MacCFSocketServer::Wakeup works as expected.
TEST(MacCFSocketServerTest, TestWakeup) {
  MacCFSocketServer server;
  WakeThread thread(&server);
  uint32 start = Time();
  thread.Start();
  server.Wait(10000, true);
  EXPECT_LT(TimeSince(start), 10000);
}

// Test that MacCarbonSocketServer::Wait works as expected.
TEST(MacCarbonSocketServerTest, TestWait) {
  MacCarbonSocketServer server;
  uint32 start = Time();
  server.Wait(1000, true);
  EXPECT_GE(TimeSince(start), 1000);
}

// Test that MacCarbonSocketServer::Wakeup works as expected.
TEST(MacCarbonSocketServerTest, TestWakeup) {
  MacCarbonSocketServer server;
  WakeThread thread(&server);
  uint32 start = Time();
  thread.Start();
  server.Wait(10000, true);
  EXPECT_LT(TimeSince(start), 10000);
}

// Test that MacCarbonAppSocketServer::Wait works as expected.
TEST(MacCarbonAppSocketServerTest, TestWait) {
  MacCarbonAppSocketServer server;
  uint32 start = Time();
  server.Wait(1000, true);
  EXPECT_GE(TimeSince(start), 1000);
}

// Test that MacCarbonAppSocketServer::Wakeup works as expected.
TEST(MacCarbonAppSocketServerTest, TestWakeup) {
  MacCarbonAppSocketServer server;
  WakeThread thread(&server);
  uint32 start = Time();
  thread.Start();
  server.Wait(10000, true);
  EXPECT_LT(TimeSince(start), 10000);
}

#endif

// Test that MacAsyncSocket passes all the generic Socket tests.
class MacAsyncSocketTest : public SocketTest {
 protected:
  MacAsyncSocketTest()
      : server_(CreateSocketServer()),
        scope_(server_.get()) {}
  // Override for other implementations of MacBaseSocketServer.
  virtual MacBaseSocketServer* CreateSocketServer() {
    return new MacCFSocketServer();
  };
  talk_base::scoped_ptr<MacBaseSocketServer> server_;
  SocketServerScope scope_;
};

TEST_F(MacAsyncSocketTest, TestConnectIPv4) {
  SocketTest::TestConnectIPv4();
}

TEST_F(MacAsyncSocketTest, TestConnectIPv6) {
  SocketTest::TestConnectIPv6();
}

TEST_F(MacAsyncSocketTest, TestConnectWithDnsLookupIPv4) {
  SocketTest::TestConnectWithDnsLookupIPv4();
}

TEST_F(MacAsyncSocketTest, TestConnectWithDnsLookupIPv6) {
  SocketTest::TestConnectWithDnsLookupIPv6();
}

TEST_F(MacAsyncSocketTest, DISABLE_TestConnectFailIPv4) {
  SocketTest::TestConnectFailIPv4();
}

TEST_F(MacAsyncSocketTest, TestConnectFailIPv6) {
  SocketTest::TestConnectFailIPv6();
}

// Reenable once we have mac async dns
TEST_F(MacAsyncSocketTest, DISABLED_TestConnectWithDnsLookupFailIPv4) {
  SocketTest::TestConnectWithDnsLookupFailIPv4();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestConnectWithDnsLookupFailIPv6) {
  SocketTest::TestConnectWithDnsLookupFailIPv6();
}

TEST_F(MacAsyncSocketTest, TestConnectWithClosedSocketIPv4) {
  SocketTest::TestConnectWithClosedSocketIPv4();
}

TEST_F(MacAsyncSocketTest, TestConnectWithClosedSocketIPv6) {
  SocketTest::TestConnectWithClosedSocketIPv6();
}

// Flaky at the moment (10% failure rate).  Seems the client doesn't get
// signalled in a timely manner...
TEST_F(MacAsyncSocketTest, DISABLED_TestServerCloseDuringConnectIPv4) {
  SocketTest::TestServerCloseDuringConnectIPv4();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestServerCloseDuringConnectIPv6) {
  SocketTest::TestServerCloseDuringConnectIPv6();
}
// Flaky at the moment (0.5% failure rate).  Seems the client doesn't get
// signalled in a timely manner...
TEST_F(MacAsyncSocketTest, TestClientCloseDuringConnectIPv4) {
  SocketTest::TestClientCloseDuringConnectIPv4();
}

TEST_F(MacAsyncSocketTest, TestClientCloseDuringConnectIPv6) {
  SocketTest::TestClientCloseDuringConnectIPv6();
}

TEST_F(MacAsyncSocketTest, TestServerCloseIPv4) {
  SocketTest::TestServerCloseIPv4();
}

TEST_F(MacAsyncSocketTest, TestServerCloseIPv6) {
  SocketTest::TestServerCloseIPv6();
}

TEST_F(MacAsyncSocketTest, TestCloseInClosedCallbackIPv4) {
  SocketTest::TestCloseInClosedCallbackIPv4();
}

TEST_F(MacAsyncSocketTest, TestCloseInClosedCallbackIPv6) {
  SocketTest::TestCloseInClosedCallbackIPv6();
}

TEST_F(MacAsyncSocketTest, TestSocketServerWaitIPv4) {
  SocketTest::TestSocketServerWaitIPv4();
}

TEST_F(MacAsyncSocketTest, TestSocketServerWaitIPv6) {
  SocketTest::TestSocketServerWaitIPv6();
}

TEST_F(MacAsyncSocketTest, TestTcpIPv4) {
  SocketTest::TestTcpIPv4();
}

TEST_F(MacAsyncSocketTest, TestTcpIPv6) {
  SocketTest::TestTcpIPv6();
}

TEST_F(MacAsyncSocketTest, TestSingleFlowControlCallbackIPv4) {
  SocketTest::TestSingleFlowControlCallbackIPv4();
}

TEST_F(MacAsyncSocketTest, TestSingleFlowControlCallbackIPv6) {
  SocketTest::TestSingleFlowControlCallbackIPv6();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestUdpIPv4) {
  SocketTest::TestUdpIPv4();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestUdpIPv6) {
  SocketTest::TestUdpIPv6();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestGetSetOptionsIPv4) {
  SocketTest::TestGetSetOptionsIPv4();
}

TEST_F(MacAsyncSocketTest, DISABLED_TestGetSetOptionsIPv6) {
  SocketTest::TestGetSetOptionsIPv6();
}

#ifndef CARBON_DEPRECATED
class MacCarbonAppAsyncSocketTest : public MacAsyncSocketTest {
  virtual MacBaseSocketServer* CreateSocketServer() {
    return new MacCarbonAppSocketServer();
  };
};

TEST_F(MacCarbonAppAsyncSocketTest, TestSocketServerWaitIPv4) {
  SocketTest::TestSocketServerWaitIPv4();
}

TEST_F(MacCarbonAppAsyncSocketTest, TestSocketServerWaitIPv6) {
  SocketTest::TestSocketServerWaitIPv6();
}
#endif
}  // namespace talk_base
