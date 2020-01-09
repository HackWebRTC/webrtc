/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/fake_network_socket_server.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {
namespace {
std::string ToString(const rtc::SocketAddress& addr) {
  return addr.HostAsURIString() + ":" + std::to_string(addr.port());
}

}  // namespace

// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket,
                          public EmulatedNetworkReceiverInterface {
 public:
  explicit FakeNetworkSocket(FakeNetworkSocketServer* scoket_manager);
  ~FakeNetworkSocket() override;

  // Will be invoked by EmulatedEndpoint to deliver packets into this socket.
  void OnPacketReceived(EmulatedIpPacket packet) override;
  // Will fire read event for incoming packets.
  bool ProcessIo();

  // rtc::Socket methods:
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Bind(const rtc::SocketAddress& addr) override;
  int Connect(const rtc::SocketAddress& addr) override;
  int Close() override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override;
  int Listen(int backlog) override;
  rtc::AsyncSocket* Accept(rtc::SocketAddress* paddr) override;
  int GetError() const override;
  void SetError(int error) override;
  ConnState GetState() const override;
  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

 private:
  absl::optional<EmulatedIpPacket> PopFrontPacket();

  FakeNetworkSocketServer* const socket_server_;
  EmulatedEndpointImpl* endpoint_;

  rtc::SocketAddress local_addr_;
  rtc::SocketAddress remote_addr_;
  ConnState state_;
  int error_;
  std::map<Option, int> options_map_;

  rtc::CriticalSection lock_;
  // Count of packets in the queue for which we didn't fire read event.
  // |pending_read_events_count_| can be different from |packet_queue_.size()|
  // because read events will be fired by one thread and packets in the queue
  // can be processed by another thread.
  int pending_read_events_count_;
  std::deque<EmulatedIpPacket> packet_queue_ RTC_GUARDED_BY(lock_);
};

FakeNetworkSocket::FakeNetworkSocket(FakeNetworkSocketServer* socket_server)
    : socket_server_(socket_server),
      state_(CS_CLOSED),
      error_(0),
      pending_read_events_count_(0) {}
FakeNetworkSocket::~FakeNetworkSocket() {
  Close();
  socket_server_->Unregister(this);
}

void FakeNetworkSocket::OnPacketReceived(EmulatedIpPacket packet) {
  {
    rtc::CritScope crit(&lock_);
    packet_queue_.push_back(std::move(packet));
    pending_read_events_count_++;
  }
  socket_server_->WakeUp();
}

bool FakeNetworkSocket::ProcessIo() {
  {
    rtc::CritScope crit(&lock_);
    if (pending_read_events_count_ == 0) {
      return false;
    }
    pending_read_events_count_--;
    RTC_DCHECK_GE(pending_read_events_count_, 0);
  }
  if (!endpoint_->Enabled()) {
    // If endpoint disabled then just pop and discard packet.
    PopFrontPacket();
    return true;
  }
  SignalReadEvent(this);
  return true;
}

rtc::SocketAddress FakeNetworkSocket::GetLocalAddress() const {
  return local_addr_;
}

rtc::SocketAddress FakeNetworkSocket::GetRemoteAddress() const {
  return remote_addr_;
}

int FakeNetworkSocket::Bind(const rtc::SocketAddress& addr) {
  RTC_CHECK(local_addr_.IsNil())
      << "Socket already bound to address: " << ToString(local_addr_);
  local_addr_ = addr;
  endpoint_ = socket_server_->GetEndpointNode(local_addr_.ipaddr());
  if (!endpoint_) {
    local_addr_.Clear();
    RTC_LOG(INFO) << "No endpoint for address: " << ToString(addr);
    error_ = EADDRNOTAVAIL;
    return 2;
  }
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(local_addr_.port(), this);
  if (!port) {
    local_addr_.Clear();
    RTC_LOG(INFO) << "Cannot bind to in-use address: " << ToString(addr);
    error_ = EADDRINUSE;
    return 1;
  }
  local_addr_.SetPort(port.value());
  return 0;
}

int FakeNetworkSocket::Connect(const rtc::SocketAddress& addr) {
  RTC_CHECK(remote_addr_.IsNil())
      << "Socket already connected to address: " << ToString(remote_addr_);
  RTC_CHECK(!local_addr_.IsNil())
      << "Socket have to be bind to some local address";
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  return 0;
}

int FakeNetworkSocket::Send(const void* pv, size_t cb) {
  RTC_CHECK(state_ == CS_CONNECTED) << "Socket cannot send: not connected";
  return SendTo(pv, cb, remote_addr_);
}

int FakeNetworkSocket::SendTo(const void* pv,
                              size_t cb,
                              const rtc::SocketAddress& addr) {
  RTC_CHECK(!local_addr_.IsNil())
      << "Socket have to be bind to some local address";
  if (!endpoint_->Enabled()) {
    error_ = ENETDOWN;
    return -1;
  }
  rtc::CopyOnWriteBuffer packet(static_cast<const uint8_t*>(pv), cb);
  endpoint_->SendPacket(local_addr_, addr, packet);
  return cb;
}

int FakeNetworkSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  rtc::SocketAddress paddr;
  return RecvFrom(pv, cb, &paddr, timestamp);
}

// Reads 1 packet from internal queue. Reads up to |cb| bytes into |pv|
// and returns the length of received packet.
int FakeNetworkSocket::RecvFrom(void* pv,
                                size_t cb,
                                rtc::SocketAddress* paddr,
                                int64_t* timestamp) {
  if (timestamp) {
    *timestamp = -1;
  }
  absl::optional<EmulatedIpPacket> packetOpt = PopFrontPacket();

  if (!packetOpt) {
    error_ = EAGAIN;
    return -1;
  }

  EmulatedIpPacket packet = std::move(packetOpt.value());
  *paddr = packet.from;
  size_t data_read = std::min(cb, packet.size());
  memcpy(pv, packet.cdata(), data_read);
  *timestamp = packet.arrival_time.us();

  // According to RECV(2) Linux Man page
  // real socket will discard data, that won't fit into provided buffer,
  // but we won't to skip such error, so we will assert here.
  RTC_CHECK(data_read == packet.size())
      << "Too small buffer is provided for socket read. "
      << "Received data size: " << packet.size()
      << "; Provided buffer size: " << cb;

  // According to RECV(2) Linux Man page
  // real socket will return message length, not data read. In our case it is
  // actually the same value.
  return static_cast<int>(packet.size());
}

int FakeNetworkSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Listen() isn't valid for SOCK_DGRAM";
}

rtc::AsyncSocket* FakeNetworkSocket::Accept(rtc::SocketAddress* /*paddr*/) {
  RTC_CHECK(false) << "Accept() isn't valid for SOCK_DGRAM";
}

int FakeNetworkSocket::Close() {
  state_ = CS_CLOSED;
  if (!local_addr_.IsNil()) {
    endpoint_->UnbindReceiver(local_addr_.port());
  }
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int FakeNetworkSocket::GetError() const {
  return error_;
}

void FakeNetworkSocket::SetError(int error) {
  RTC_CHECK(error == 0);
  error_ = error;
}

rtc::AsyncSocket::ConnState FakeNetworkSocket::GetState() const {
  return state_;
}

int FakeNetworkSocket::GetOption(Option opt, int* value) {
  auto it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int FakeNetworkSocket::SetOption(Option opt, int value) {
  options_map_[opt] = value;
  return 0;
}

absl::optional<EmulatedIpPacket> FakeNetworkSocket::PopFrontPacket() {
  rtc::CritScope crit(&lock_);
  if (packet_queue_.empty()) {
    return absl::nullopt;
  }

  absl::optional<EmulatedIpPacket> packet =
      absl::make_optional(std::move(packet_queue_.front()));
  packet_queue_.pop_front();
  return packet;
}

FakeNetworkSocketServer::FakeNetworkSocketServer(
    Clock* clock,
    EndpointsContainer* endpoints_container)
    : clock_(clock),
      endpoints_container_(endpoints_container),
      wakeup_(/*manual_reset=*/false, /*initially_signaled=*/false) {}
FakeNetworkSocketServer::~FakeNetworkSocketServer() = default;

void FakeNetworkSocketServer::OnMessageQueueDestroyed() {
  msg_queue_ = nullptr;
}

EmulatedEndpointImpl* FakeNetworkSocketServer::GetEndpointNode(
    const rtc::IPAddress& ip) {
  return endpoints_container_->LookupByLocalAddress(ip);
}

void FakeNetworkSocketServer::Unregister(FakeNetworkSocket* socket) {
  rtc::CritScope crit(&lock_);
  sockets_.erase(absl::c_find(sockets_, socket));
}

rtc::Socket* FakeNetworkSocketServer::CreateSocket(int /*family*/,
                                                   int /*type*/) {
  RTC_CHECK(false) << "Only async sockets are supported";
}

rtc::AsyncSocket* FakeNetworkSocketServer::CreateAsyncSocket(int family,
                                                             int type) {
  RTC_DCHECK(family == AF_INET || family == AF_INET6);
  // We support only UDP sockets for now.
  RTC_DCHECK(type == SOCK_DGRAM) << "Only UDP sockets are supported";
  FakeNetworkSocket* out = new FakeNetworkSocket(this);
  {
    rtc::CritScope crit(&lock_);
    sockets_.push_back(out);
  }
  return out;
}

void FakeNetworkSocketServer::SetMessageQueue(rtc::Thread* msg_queue) {
  msg_queue_ = msg_queue;
  if (msg_queue_) {
    msg_queue_->SignalQueueDestroyed.connect(
        this, &FakeNetworkSocketServer::OnMessageQueueDestroyed);
  }
}

// Always returns true (if return false, it won't be invoked again...)
bool FakeNetworkSocketServer::Wait(int cms, bool process_io) {
  RTC_DCHECK(msg_queue_ == rtc::Thread::Current());
  if (!process_io) {
    wakeup_.Wait(cms);
    return true;
  }
  wakeup_.Wait(cms);

  rtc::CritScope crit(&lock_);
  for (auto* socket : sockets_) {
    while (socket->ProcessIo()) {
    }
  }
  return true;
}

void FakeNetworkSocketServer::WakeUp() {
  wakeup_.Set();
}

Timestamp FakeNetworkSocketServer::Now() const {
  return clock_->CurrentTime();
}

}  // namespace test
}  // namespace webrtc
