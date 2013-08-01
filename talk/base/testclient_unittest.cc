/*
 * libjingle
 * Copyright 2006, Google Inc.
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
#include "talk/base/nethelpers.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/testclient.h"
#include "talk/base/testechoserver.h"
#include "talk/base/thread.h"

using namespace talk_base;

void TestUdpInternal(const SocketAddress& loopback) {
  Thread *main = Thread::Current();
  AsyncSocket* socket = main->socketserver()
      ->CreateAsyncSocket(loopback.family(), SOCK_DGRAM);
  socket->Bind(loopback);

  TestClient client(new AsyncUDPSocket(socket));
  SocketAddress addr = client.address(), from;
  EXPECT_EQ(3, client.SendTo("foo", 3, addr));
  EXPECT_TRUE(client.CheckNextPacket("foo", 3, &from));
  EXPECT_EQ(from, addr);
  EXPECT_TRUE(client.CheckNoPacket());
}

void TestTcpInternal(const SocketAddress& loopback) {
  Thread *main = Thread::Current();
  TestEchoServer server(main, loopback);

  AsyncSocket* socket = main->socketserver()
      ->CreateAsyncSocket(loopback.family(), SOCK_STREAM);
  AsyncTCPSocket* tcp_socket = AsyncTCPSocket::Create(
      socket, loopback, server.address());
  ASSERT_TRUE(tcp_socket != NULL);

  TestClient client(tcp_socket);
  SocketAddress addr = client.address(), from;
  EXPECT_TRUE(client.CheckConnected());
  EXPECT_EQ(3, client.Send("foo", 3));
  EXPECT_TRUE(client.CheckNextPacket("foo", 3, &from));
  EXPECT_EQ(from, server.address());
  EXPECT_TRUE(client.CheckNoPacket());
}

// Tests whether the TestClient can send UDP to itself.
TEST(TestClientTest, TestUdpIPv4) {
  TestUdpInternal(SocketAddress("127.0.0.1", 0));
}

TEST(TestClientTest, TestUdpIPv6) {
  if (HasIPv6Enabled()) {
    TestUdpInternal(SocketAddress("::1", 0));
  } else {
    LOG(LS_INFO) << "Skipping IPv6 test.";
  }
}

// Tests whether the TestClient can connect to a server and exchange data.
TEST(TestClientTest, TestTcpIPv4) {
  TestTcpInternal(SocketAddress("127.0.0.1", 0));
}

TEST(TestClientTest, TestTcpIPv6) {
  if (HasIPv6Enabled()) {
    TestTcpInternal(SocketAddress("::1", 0));
  } else {
    LOG(LS_INFO) << "Skipping IPv6 test.";
  }
}
