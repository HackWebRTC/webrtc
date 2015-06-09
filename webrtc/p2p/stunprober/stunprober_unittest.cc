/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/basictypes.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/virtualsocketserver.h"

#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/stunprober/stunprober.h"
#include "webrtc/p2p/stunprober/stunprober_dependencies.h"

using stunprober::HostNameResolverInterface;
using stunprober::TaskRunner;
using stunprober::SocketFactory;
using stunprober::StunProber;
using stunprober::AsyncCallback;
using stunprober::ClientSocketInterface;
using stunprober::ServerSocketInterface;
using stunprober::SocketFactory;
using stunprober::TaskRunner;

namespace stunprober {

namespace {

const rtc::SocketAddress kLocalAddr("192.168.0.1", 0);
const rtc::SocketAddress kStunAddr1("1.1.1.1", 3478);
const rtc::SocketAddress kStunAddr2("1.1.1.2", 3478);
const rtc::SocketAddress kFailedStunAddr("1.1.1.3", 3478);
const rtc::SocketAddress kStunMappedAddr("77.77.77.77", 0);

class TestSocketServer : public rtc::VirtualSocketServer {
 public:
  using rtc::VirtualSocketServer::CreateAsyncSocket;
  explicit TestSocketServer(SocketServer* ss) : rtc::VirtualSocketServer(ss) {}
  void SetLocalAddress(const rtc::SocketAddress& addr) { addr_ = addr; }

  // CreateAsyncSocket is used by StunProber to create both client and server
  // sockets. The first socket is used to retrieve local address which will be
  // used later for Bind().
  rtc::AsyncSocket* CreateAsyncSocket(int type) override {
    rtc::VirtualSocket* socket = static_cast<rtc::VirtualSocket*>(
        rtc::VirtualSocketServer::CreateAsyncSocket(type));
    if (!local_addr_set_) {
      // Only the first socket can SetLocalAddress. For others, Bind will fail
      // if local address is set.
      socket->SetLocalAddress(addr_);
      local_addr_set_ = true;
    } else {
      sockets_.push_back(socket);
    }
    return socket;
  }

  size_t num_socket() { return sockets_.size(); }

 private:
  bool local_addr_set_ = false;
  std::vector<rtc::VirtualSocket*> sockets_;
  rtc::SocketAddress addr_;
};

class FakeHostNameResolver : public HostNameResolverInterface {
 public:
  FakeHostNameResolver() {}
  void set_result(int ret) { ret_ = ret; }
  void set_addresses(const std::vector<rtc::SocketAddress>& addresses) {
    server_ips_ = addresses;
  }
  const std::vector<rtc::SocketAddress>& get_addresses() { return server_ips_; }
  void add_address(const rtc::SocketAddress& ip) { server_ips_.push_back(ip); }
  void Resolve(const rtc::SocketAddress& addr,
               std::vector<rtc::SocketAddress>* addresses,
               stunprober::AsyncCallback callback) override {
    *addresses = server_ips_;
    callback(ret_);
  }

 private:
  int ret_ = 0;
  std::vector<rtc::SocketAddress> server_ips_;
};

}  // namespace

class StunProberTest : public testing::Test {
 public:
  StunProberTest()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        ss_(new TestSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        result_(StunProber::SUCCESS),
        stun_server_1_(cricket::TestStunServer::Create(rtc::Thread::Current(),
                                                       kStunAddr1)),
        stun_server_2_(cricket::TestStunServer::Create(rtc::Thread::Current(),
                                                       kStunAddr2)) {
    stun_server_1_->set_fake_stun_addr(kStunMappedAddr);
    stun_server_2_->set_fake_stun_addr(kStunMappedAddr);
    rtc::InitializeSSL();
  }

  void set_expected_result(int result) { result_ = result; }

  void StartProbing(HostNameResolverInterface* resolver,
                    SocketFactoryInterface* socket_factory,
                    const rtc::SocketAddress& addr,
                    bool shared_socket,
                    uint16 interval,
                    uint16 pings_per_ip) {
    std::vector<rtc::SocketAddress> addrs;
    addrs.push_back(addr);
    prober.reset(new StunProber(resolver, socket_factory, new TaskRunner()));
    prober->Start(addrs, shared_socket, interval, pings_per_ip,
                  100 /* timeout_ms */,
                  [this](int result) { this->StopCallback(result); });
  }

  void RunProber(bool shared_mode) {
    const int pings_per_ip = 3;
    const uint16 port = kStunAddr1.port();
    rtc::SocketAddress addr("stun.l.google.com", port);
    std::vector<rtc::SocketAddress> addrs;

    // Set up the resolver for 2 stun server addresses.
    rtc::scoped_ptr<FakeHostNameResolver> resolver(new FakeHostNameResolver());
    resolver->add_address(kStunAddr1);
    resolver->add_address(kStunAddr2);
    // Add a non-existing server. This shouldn't pollute the result.
    resolver->add_address(kFailedStunAddr);

    rtc::scoped_ptr<SocketFactory> socket_factory(new SocketFactory());

    // Set local address in socketserver so getsockname will return kLocalAddr
    // instead of 0.0.0.0 for the first socket.
    ss_->SetLocalAddress(kLocalAddr);

    // Set up the expected results for verification.
    std::set<std::string> srflx_addresses;
    srflx_addresses.insert(kStunMappedAddr.ToString());
    const uint32 total_pings_tried =
        static_cast<uint32>(pings_per_ip * resolver->get_addresses().size());

    // The reported total_pings should not count for pings sent to the
    // kFailedStunAddr.
    const uint32 total_pings_reported = total_pings_tried - pings_per_ip;

    size_t total_sockets = shared_mode ? pings_per_ip : total_pings_tried;

    StartProbing(resolver.release(), socket_factory.release(), addr,
                 shared_mode, 3, pings_per_ip);

    WAIT(stopped_, 1000);

    StunProber::Stats stats;
    EXPECT_EQ(ss_->num_socket(), total_sockets);
    EXPECT_TRUE(prober->GetStats(&stats));
    EXPECT_EQ(stats.success_percent, 100);
    EXPECT_TRUE(stats.nat_type > stunprober::NATTYPE_NONE);
    EXPECT_EQ(stats.host_ip, kLocalAddr.ipaddr().ToString());
    EXPECT_EQ(stats.srflx_addrs, srflx_addresses);
    EXPECT_EQ(static_cast<uint32>(stats.num_request_sent),
              total_pings_reported);
    EXPECT_EQ(static_cast<uint32>(stats.num_response_received),
              total_pings_reported);
  }

 private:
  void StopCallback(int result) {
    EXPECT_EQ(result, result_);
    stopped_ = true;
  }

  rtc::Thread* main_;
  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<TestSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::scoped_ptr<StunProber> prober;
  int result_ = 0;
  bool stopped_ = false;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_1_;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_2_;
};

TEST_F(StunProberTest, DNSFailure) {
  rtc::SocketAddress addr("stun.l.google.com", 19302);
  rtc::scoped_ptr<FakeHostNameResolver> resolver(new FakeHostNameResolver());
  rtc::scoped_ptr<SocketFactory> socket_factory(new SocketFactory());

  set_expected_result(StunProber::RESOLVE_FAILED);

  // Non-0 value is treated as failure.
  resolver->set_result(1);
  StartProbing(resolver.release(), socket_factory.release(), addr, false, 10,
               30);
}

TEST_F(StunProberTest, NonSharedMode) {
  RunProber(false);
}

TEST_F(StunProberTest, SharedMode) {
  RunProber(true);
}

}  // namespace stunprober
