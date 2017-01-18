/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/api/ortcfactoryinterface.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/p2p/base/udptransport.h"

namespace {

const int kDefaultTimeout = 10000;  // 10 seconds.
static const rtc::IPAddress kIPv4LocalHostAddress =
    rtc::IPAddress(0x7F000001);  // 127.0.0.1

class PacketReceiver : public sigslot::has_slots<> {
 public:
  explicit PacketReceiver(rtc::PacketTransportInterface* transport) {
    transport->SignalReadPacket.connect(this, &PacketReceiver::OnReadPacket);
  }
  int packets_read() const { return packets_read_; }

 private:
  void OnReadPacket(rtc::PacketTransportInterface*,
                    const char*,
                    size_t,
                    const rtc::PacketTime&,
                    int) {
    ++packets_read_;
  }

  int packets_read_ = 0;
};

}  // namespace

namespace webrtc {

// Used to test that things work end-to-end when using the default
// implementations of threads/etc. provided by OrtcFactory, with the exception
// of using a virtual network.
//
// By default, the virtual network manager doesn't enumerate any networks, but
// sockets can still be created in this state.
class OrtcFactoryTest : public testing::Test {
 public:
  OrtcFactoryTest()
      : virtual_socket_server_(&physical_socket_server_),
        network_thread_(&virtual_socket_server_),
        ortc_factory_(OrtcFactoryInterface::Create(&network_thread_,
                                                   nullptr,
                                                   &fake_network_manager_,
                                                   nullptr)) {
    // Sockets are bound to the ANY address, so this is needed to tell the
    // virtual network which address to use in this case.
    virtual_socket_server_.SetDefaultRoute(kIPv4LocalHostAddress);
    network_thread_.Start();
  }

 protected:
  rtc::PhysicalSocketServer physical_socket_server_;
  rtc::VirtualSocketServer virtual_socket_server_;
  rtc::Thread network_thread_;
  rtc::FakeNetworkManager fake_network_manager_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
};

TEST_F(OrtcFactoryTest, EndToEndUdpTransport) {
  std::unique_ptr<UdpTransportInterface> transport1 =
      ortc_factory_->CreateUdpTransport(AF_INET);
  std::unique_ptr<UdpTransportInterface> transport2 =
      ortc_factory_->CreateUdpTransport(AF_INET);
  ASSERT_NE(nullptr, transport1);
  ASSERT_NE(nullptr, transport2);
  // Sockets are bound to the ANY address, so we need to provide the IP address
  // explicitly.
  transport1->SetRemoteAddress(
      rtc::SocketAddress(virtual_socket_server_.GetDefaultRoute(AF_INET),
                         transport2->GetLocalAddress().port()));
  transport2->SetRemoteAddress(
      rtc::SocketAddress(virtual_socket_server_.GetDefaultRoute(AF_INET),
                         transport1->GetLocalAddress().port()));

  // TODO(deadbeef): Once there's something (RTP senders/receivers) that can
  // use UdpTransport end-to-end, use that for this end-to-end test instead of
  // making assumptions about the implementation.
  //
  // For now, this assumes the returned object is a UdpTransportProxy that wraps
  // a UdpTransport.
  cricket::UdpTransport* internal_transport1 =
      static_cast<UdpTransportProxyWithInternal<cricket::UdpTransport>*>(
          transport1.get())
          ->internal();
  cricket::UdpTransport* internal_transport2 =
      static_cast<UdpTransportProxyWithInternal<cricket::UdpTransport>*>(
          transport2.get())
          ->internal();
  // Need to call internal "SendPacket" method on network thread.
  network_thread_.Invoke<void>(
      RTC_FROM_HERE, [internal_transport1, internal_transport2]() {
        PacketReceiver receiver1(internal_transport1);
        PacketReceiver receiver2(internal_transport2);
        internal_transport1->SendPacket("foo", sizeof("foo"),
                                        rtc::PacketOptions(), 0);
        internal_transport2->SendPacket("foo", sizeof("foo"),
                                        rtc::PacketOptions(), 0);
        EXPECT_EQ_WAIT(1, receiver1.packets_read(), kDefaultTimeout);
        EXPECT_EQ_WAIT(1, receiver2.packets_read(), kDefaultTimeout);
      });
}

}  // namespace webrtc
