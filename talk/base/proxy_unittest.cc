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

#include <string>
#include "talk/base/autodetectproxy.h"
#include "talk/base/gunit.h"
#include "talk/base/httpserver.h"
#include "talk/base/proxyserver.h"
#include "talk/base/socketadapters.h"
#include "talk/base/testclient.h"
#include "talk/base/testechoserver.h"
#include "talk/base/virtualsocketserver.h"

using talk_base::Socket;
using talk_base::Thread;
using talk_base::SocketAddress;

static const SocketAddress kSocksProxyIntAddr("1.2.3.4", 1080);
static const SocketAddress kSocksProxyExtAddr("1.2.3.5", 0);
static const SocketAddress kHttpsProxyIntAddr("1.2.3.4", 443);
static const SocketAddress kHttpsProxyExtAddr("1.2.3.5", 0);
static const SocketAddress kBogusProxyIntAddr("1.2.3.4", 999);

// Used to run a proxy detect on the current thread. Otherwise we would need
// to make both threads share the same VirtualSocketServer.
class AutoDetectProxyRunner : public talk_base::AutoDetectProxy {
 public:
  explicit AutoDetectProxyRunner(const std::string& agent)
      : AutoDetectProxy(agent) {}
  void Run() {
    DoWork();
    Thread::Current()->Restart();  // needed to reset the messagequeue
  }
};

// Sets up a virtual socket server and HTTPS/SOCKS5 proxy servers.
class ProxyTest : public testing::Test {
 public:
  ProxyTest() : ss_(new talk_base::VirtualSocketServer(NULL)) {
    Thread::Current()->set_socketserver(ss_.get());
    socks_.reset(new talk_base::SocksProxyServer(
        ss_.get(), kSocksProxyIntAddr, ss_.get(), kSocksProxyExtAddr));
    https_.reset(new talk_base::HttpListenServer());
    https_->Listen(kHttpsProxyIntAddr);
  }
  ~ProxyTest() {
    Thread::Current()->set_socketserver(NULL);
  }

  talk_base::SocketServer* ss() { return ss_.get(); }

  talk_base::ProxyType DetectProxyType(const SocketAddress& address) {
    talk_base::ProxyType type;
    AutoDetectProxyRunner* detect = new AutoDetectProxyRunner("unittest/1.0");
    detect->set_proxy(address);
    detect->Run();  // blocks until done
    type = detect->proxy().type;
    detect->Destroy(false);
    return type;
  }

 private:
  talk_base::scoped_ptr<talk_base::SocketServer> ss_;
  talk_base::scoped_ptr<talk_base::SocksProxyServer> socks_;
  // TODO: Make this a real HTTPS proxy server.
  talk_base::scoped_ptr<talk_base::HttpListenServer> https_;
};

// Tests whether we can use a SOCKS5 proxy to connect to a server.
TEST_F(ProxyTest, TestSocks5Connect) {
  talk_base::AsyncSocket* socket =
      ss()->CreateAsyncSocket(kSocksProxyIntAddr.family(), SOCK_STREAM);
  talk_base::AsyncSocksProxySocket* proxy_socket =
      new talk_base::AsyncSocksProxySocket(socket, kSocksProxyIntAddr,
                                           "", talk_base::CryptString());
  // TODO: IPv6-ize these tests when proxy supports IPv6.

  talk_base::TestEchoServer server(Thread::Current(),
                                   SocketAddress(INADDR_ANY, 0));

  talk_base::AsyncTCPSocket* packet_socket = talk_base::AsyncTCPSocket::Create(
      proxy_socket, SocketAddress(INADDR_ANY, 0), server.address());
  EXPECT_TRUE(packet_socket != NULL);
  talk_base::TestClient client(packet_socket);

  EXPECT_EQ(Socket::CS_CONNECTING, proxy_socket->GetState());
  EXPECT_TRUE(client.CheckConnected());
  EXPECT_EQ(Socket::CS_CONNECTED, proxy_socket->GetState());
  EXPECT_EQ(server.address(), client.remote_address());
  client.Send("foo", 3);
  EXPECT_TRUE(client.CheckNextPacket("foo", 3, NULL));
  EXPECT_TRUE(client.CheckNoPacket());
}

/*
// Tests whether we can use a HTTPS proxy to connect to a server.
TEST_F(ProxyTest, TestHttpsConnect) {
  AsyncSocket* socket = ss()->CreateAsyncSocket(SOCK_STREAM);
  AsyncHttpsProxySocket* proxy_socket = new AsyncHttpsProxySocket(
      socket, "unittest/1.0", kHttpsProxyIntAddress, "", CryptString());
  TestClient client(new AsyncTCPSocket(proxy_socket));
  TestEchoServer server(Thread::Current(), SocketAddress());

  EXPECT_TRUE(client.Connect(server.address()));
  EXPECT_TRUE(client.CheckConnected());
  EXPECT_EQ(server.address(), client.remote_address());
  client.Send("foo", 3);
  EXPECT_TRUE(client.CheckNextPacket("foo", 3, NULL));
  EXPECT_TRUE(client.CheckNoPacket());
}
*/

// Tests whether we can autodetect a SOCKS5 proxy.
TEST_F(ProxyTest, TestAutoDetectSocks5) {
  EXPECT_EQ(talk_base::PROXY_SOCKS5, DetectProxyType(kSocksProxyIntAddr));
}

/*
// Tests whether we can autodetect a HTTPS proxy.
TEST_F(ProxyTest, TestAutoDetectHttps) {
  EXPECT_EQ(talk_base::PROXY_HTTPS, DetectProxyType(kHttpsProxyIntAddr));
}
*/

// Tests whether we fail properly for no proxy.
TEST_F(ProxyTest, TestAutoDetectBogus) {
  EXPECT_EQ(talk_base::PROXY_UNKNOWN, DetectProxyType(kBogusProxyIntAddr));
}
