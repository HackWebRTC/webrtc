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

#include "talk/base/logging.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/socketadapters.h"
#include "talk/base/socketaddress.h"
#include "talk/base/ssladapter.h"
#include "talk/base/thread.h"
#include "talk/base/virtualsocketserver.h"
#include "talk/p2p/base/basicpacketsocketfactory.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/relayserver.h"

using talk_base::SocketAddress;

static const SocketAddress kLocalAddress = SocketAddress("192.168.1.2", 0);
static const SocketAddress kRelayUdpAddr = SocketAddress("99.99.99.1", 5000);
static const SocketAddress kRelayTcpAddr = SocketAddress("99.99.99.2", 5001);
static const SocketAddress kRelaySslAddr = SocketAddress("99.99.99.3", 443);
static const SocketAddress kRelayExtAddr = SocketAddress("99.99.99.3", 5002);

static const int kTimeoutMs = 1000;
static const int kMaxTimeoutMs = 5000;

// Tests connecting a RelayPort to a fake relay server
// (cricket::RelayServer) using all currently available protocols. The
// network layer is faked out by using a VirtualSocketServer for
// creating sockets. The test will monitor the current state of the
// RelayPort and created sockets by listening for signals such as,
// SignalConnectFailure, SignalConnectTimeout, SignalSocketClosed and
// SignalReadPacket.
class RelayPortTest : public testing::Test,
                      public sigslot::has_slots<> {
 public:
  RelayPortTest()
      : main_(talk_base::Thread::Current()),
        physical_socket_server_(new talk_base::PhysicalSocketServer),
        virtual_socket_server_(new talk_base::VirtualSocketServer(
            physical_socket_server_.get())),
        ss_scope_(virtual_socket_server_.get()),
        network_("unittest", "unittest", talk_base::IPAddress(INADDR_ANY), 32),
        socket_factory_(talk_base::Thread::Current()),
        username_(talk_base::CreateRandomString(16)),
        password_(talk_base::CreateRandomString(16)),
        relay_port_(cricket::RelayPort::Create(main_, &socket_factory_,
                                               &network_,
                                               kLocalAddress.ipaddr(),
                                               0, 0, username_, password_)),
        relay_server_(new cricket::RelayServer(main_)) {
  }

  void OnReadPacket(talk_base::AsyncPacketSocket* socket,
                    const char* data, size_t size,
                    const talk_base::SocketAddress& remote_addr,
                    const talk_base::PacketTime& packet_time) {
    received_packet_count_[socket]++;
  }

  void OnConnectFailure(const cricket::ProtocolAddress* addr) {
    failed_connections_.push_back(*addr);
  }

  void OnSoftTimeout(const cricket::ProtocolAddress* addr) {
    soft_timedout_connections_.push_back(*addr);
  }

 protected:
  static void SetUpTestCase() {
    talk_base::InitializeSSL();
  }

  static void TearDownTestCase() {
    talk_base::CleanupSSL();
  }


  virtual void SetUp() {
    // The relay server needs an external socket to work properly.
    talk_base::AsyncUDPSocket* ext_socket =
        CreateAsyncUdpSocket(kRelayExtAddr);
    relay_server_->AddExternalSocket(ext_socket);

    // Listen for failures.
    relay_port_->SignalConnectFailure.
        connect(this, &RelayPortTest::OnConnectFailure);

    // Listen for soft timeouts.
    relay_port_->SignalSoftTimeout.
        connect(this, &RelayPortTest::OnSoftTimeout);
  }

  // Udp has the highest 'goodness' value of the three different
  // protocols used for connecting to the relay server. As soon as
  // PrepareAddress is called, the RelayPort will start trying to
  // connect to the given UDP address. As soon as a response to the
  // sent STUN allocate request message has been received, the
  // RelayPort will consider the connection to be complete and will
  // abort any other connection attempts.
  void TestConnectUdp() {
    // Add a UDP socket to the relay server.
    talk_base::AsyncUDPSocket* internal_udp_socket =
        CreateAsyncUdpSocket(kRelayUdpAddr);
    talk_base::AsyncSocket* server_socket = CreateServerSocket(kRelayTcpAddr);

    relay_server_->AddInternalSocket(internal_udp_socket);
    relay_server_->AddInternalServerSocket(server_socket, cricket::PROTO_TCP);

    // Now add our relay addresses to the relay port and let it start.
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayUdpAddr, cricket::PROTO_UDP));
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP));
    relay_port_->PrepareAddress();

    // Should be connected.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kTimeoutMs);

    // Make sure that we are happy with UDP, ie. not continuing with
    // TCP, SSLTCP, etc.
    WAIT(relay_server_->HasConnection(kRelayTcpAddr), kTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the UDP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelayUdpAddr));
  }

  // TCP has the second best 'goodness' value, and as soon as UDP
  // connection has failed, the RelayPort will attempt to connect via
  // TCP. Here we add a fake UDP address together with a real TCP
  // address to simulate an UDP failure. As soon as UDP has failed the
  // RelayPort will try the TCP adress and succed.
  void TestConnectTcp() {
    // Create a fake UDP address for relay port to simulate a failure.
    cricket::ProtocolAddress fake_protocol_address =
        cricket::ProtocolAddress(kRelayUdpAddr, cricket::PROTO_UDP);

    // Create a server socket for the RelayServer.
    talk_base::AsyncSocket* server_socket = CreateServerSocket(kRelayTcpAddr);
    relay_server_->AddInternalServerSocket(server_socket, cricket::PROTO_TCP);

    // Add server addresses to the relay port and let it start.
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(fake_protocol_address));
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP));
    relay_port_->PrepareAddress();

    EXPECT_FALSE(relay_port_->IsReady());

    // Should have timed out in 200 + 200 + 400 + 800 + 1600 ms.
    EXPECT_TRUE_WAIT(HasFailed(&fake_protocol_address), 3600);

    // Wait until relayport is ready.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kMaxTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the TCP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelayTcpAddr));
  }

  void TestConnectSslTcp() {
    // Create a fake TCP address for relay port to simulate a failure.
    // We skip UDP here since transition from UDP to TCP has been
    // tested above.
    cricket::ProtocolAddress fake_protocol_address =
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP);

    // Create a ssl server socket for the RelayServer.
    talk_base::AsyncSocket* ssl_server_socket =
        CreateServerSocket(kRelaySslAddr);
    relay_server_->AddInternalServerSocket(ssl_server_socket,
                                           cricket::PROTO_SSLTCP);

    // Create a tcp server socket that listens on the fake address so
    // the relay port can attempt to connect to it.
    talk_base::scoped_ptr<talk_base::AsyncSocket> tcp_server_socket(
        CreateServerSocket(kRelayTcpAddr));

    // Add server addresses to the relay port and let it start.
    relay_port_->AddServerAddress(fake_protocol_address);
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelaySslAddr, cricket::PROTO_SSLTCP));
    relay_port_->PrepareAddress();
    EXPECT_FALSE(relay_port_->IsReady());

    // Should have timed out in 3000 ms(relayport.cc, kSoftConnectTimeoutMs).
    EXPECT_TRUE_WAIT_MARGIN(HasTimedOut(&fake_protocol_address), 3000, 100);

    // Wait until relayport is ready.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kMaxTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the SSLTCP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelaySslAddr));
  }

 private:
  talk_base::AsyncUDPSocket* CreateAsyncUdpSocket(const SocketAddress addr) {
    talk_base::AsyncSocket* socket =
        virtual_socket_server_->CreateAsyncSocket(SOCK_DGRAM);
    talk_base::AsyncUDPSocket* packet_socket =
        talk_base::AsyncUDPSocket::Create(socket, addr);
    EXPECT_TRUE(packet_socket != NULL);
    packet_socket->SignalReadPacket.connect(this, &RelayPortTest::OnReadPacket);
    return packet_socket;
  }

  talk_base::AsyncSocket* CreateServerSocket(const SocketAddress addr) {
    talk_base::AsyncSocket* socket =
        virtual_socket_server_->CreateAsyncSocket(SOCK_STREAM);
    EXPECT_GE(socket->Bind(addr), 0);
    EXPECT_GE(socket->Listen(5), 0);
    return socket;
  }

  bool HasFailed(cricket::ProtocolAddress* addr) {
    for (size_t i = 0; i < failed_connections_.size(); i++) {
      if (failed_connections_[i].address == addr->address &&
          failed_connections_[i].proto == addr->proto) {
        return true;
      }
    }
    return false;
  }

  bool HasTimedOut(cricket::ProtocolAddress* addr) {
    for (size_t i = 0; i < soft_timedout_connections_.size(); i++) {
      if (soft_timedout_connections_[i].address == addr->address &&
          soft_timedout_connections_[i].proto == addr->proto) {
        return true;
      }
    }
    return false;
  }

  typedef std::map<talk_base::AsyncPacketSocket*, int> PacketMap;

  talk_base::Thread* main_;
  talk_base::scoped_ptr<talk_base::PhysicalSocketServer>
      physical_socket_server_;
  talk_base::scoped_ptr<talk_base::VirtualSocketServer> virtual_socket_server_;
  talk_base::SocketServerScope ss_scope_;
  talk_base::Network network_;
  talk_base::BasicPacketSocketFactory socket_factory_;
  std::string username_;
  std::string password_;
  talk_base::scoped_ptr<cricket::RelayPort> relay_port_;
  talk_base::scoped_ptr<cricket::RelayServer> relay_server_;
  std::vector<cricket::ProtocolAddress> failed_connections_;
  std::vector<cricket::ProtocolAddress> soft_timedout_connections_;
  PacketMap received_packet_count_;
};

TEST_F(RelayPortTest, ConnectUdp) {
  TestConnectUdp();
}

TEST_F(RelayPortTest, ConnectTcp) {
  TestConnectTcp();
}

TEST_F(RelayPortTest, ConnectSslTcp) {
  TestConnectSslTcp();
}
