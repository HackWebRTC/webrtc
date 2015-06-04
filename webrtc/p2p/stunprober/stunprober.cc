/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <map>
#include <set>
#include <string>

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/p2p/stunprober/stunprober.h"

namespace stunprober {

namespace {

void IncrementCounterByAddress(std::map<rtc::IPAddress, int>* counter_per_ip,
                               const rtc::IPAddress& ip) {
  counter_per_ip->insert(std::make_pair(ip, 0)).first->second++;
}

}  // namespace

StunProber::Requester::Requester(
    StunProber* prober,
    ServerSocketInterface* socket,
    const std::vector<rtc::SocketAddress>& server_ips)
    : prober_(prober),
      socket_(socket),
      response_packet_(new rtc::ByteBuffer(nullptr, kMaxUdpBufferSize)),
      server_ips_(server_ips),
      thread_checker_(prober->thread_checker_) {
}

StunProber::Requester::~Requester() {
  if (socket_) {
    socket_->Close();
  }
  for (auto req : requests_) {
    if (req) {
      delete req;
    }
  }
}

void StunProber::Requester::SendStunRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  requests_.push_back(new Request());
  Request& request = *(requests_.back());
  cricket::StunMessage message;

  // Random transaction ID, STUN_BINDING_REQUEST
  message.SetTransactionID(
      rtc::CreateRandomString(cricket::kStunTransactionIdLength));
  message.SetType(cricket::STUN_BINDING_REQUEST);

  rtc::scoped_ptr<rtc::ByteBuffer> request_packet(
      new rtc::ByteBuffer(nullptr, kMaxUdpBufferSize));
  if (!message.Write(request_packet.get())) {
    prober_->End(WRITE_FAILED, 0);
    return;
  }

  auto addr = server_ips_[num_request_sent_];
  request.server_addr = addr.ipaddr();

  // The write must succeed immediately. Otherwise, the calculating of the STUN
  // request timing could become too complicated. Callback is ignored by passing
  // empty AsyncCallback.
  int rv = socket_->SendTo(addr, const_cast<char*>(request_packet->Data()),
                           request_packet->Length(), AsyncCallback());
  if (rv < 0) {
    prober_->End(WRITE_FAILED, rv);
    return;
  }

  request.sent_time_ms = rtc::Time();

  // Post a read waiting for response. For share mode, the subsequent read will
  // be posted inside OnStunResponseReceived.
  if (num_request_sent_ == 0) {
    ReadStunResponse();
  }

  num_request_sent_++;
  DCHECK(static_cast<size_t>(num_request_sent_) <= server_ips_.size());
}

void StunProber::Requester::ReadStunResponse() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!socket_) {
    return;
  }

  int rv = socket_->RecvFrom(
      response_packet_->ReserveWriteBuffer(kMaxUdpBufferSize),
      kMaxUdpBufferSize, &addr_,
      [this](int result) { this->OnStunResponseReceived(result); });
  if (rv != SocketInterface::IO_PENDING) {
    OnStunResponseReceived(rv);
  }
}

void StunProber::Requester::Request::ProcessResponse(
    rtc::ByteBuffer* message,
    int buf_len,
    const rtc::IPAddress& local_addr) {
  int64 now = rtc::Time();

  cricket::StunMessage stun_response;
  if (!stun_response.Read(message)) {
    // Invalid or incomplete STUN packet.
    received_time_ms = 0;
    return;
  }

  // Get external address of the socket.
  const cricket::StunAddressAttribute* addr_attr =
      stun_response.GetAddress(cricket::STUN_ATTR_MAPPED_ADDRESS);
  if (addr_attr == nullptr) {
    // Addresses not available to detect whether or not behind a NAT.
    return;
  }

  if (addr_attr->family() != cricket::STUN_ADDRESS_IPV4 &&
      addr_attr->family() != cricket::STUN_ADDRESS_IPV6) {
    return;
  }

  received_time_ms = now;

  srflx_addr = addr_attr->GetAddress();

  // Calculate behind_nat.
  behind_nat = (srflx_addr.ipaddr() != local_addr);
}

void StunProber::Requester::OnStunResponseReceived(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(socket_);

  if (result < 0) {
    // Something is wrong, finish the test.
    prober_->End(READ_FAILED, result);
    return;
  }

  Request* request = GetRequestByAddress(addr_.ipaddr());
  if (!request) {
    // Something is wrong, finish the test.
    prober_->End(GENERIC_FAILURE, result);
    return;
  }

  num_response_received_++;

  // Resize will set the end_ to indicate that there are data available in this
  // ByteBuffer.
  response_packet_->Resize(result);
  request->ProcessResponse(response_packet_.get(), result,
                           prober_->local_addr_);

  if (static_cast<size_t>(num_response_received_) < server_ips_.size()) {
    // Post another read response.
    ReadStunResponse();
  }
}

StunProber::Requester::Request* StunProber::Requester::GetRequestByAddress(
    const rtc::IPAddress& ipaddr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto request : requests_) {
    if (request->server_addr == ipaddr) {
      return request;
    }
  }

  return nullptr;
}

StunProber::StunProber(HostNameResolverInterface* host_name_resolver,
                       SocketFactoryInterface* socket_factory,
                       TaskRunnerInterface* task_runner)
    : interval_ms_(0),
      socket_factory_(socket_factory),
      resolver_(host_name_resolver),
      task_runner_(task_runner) {
}

StunProber::~StunProber() {
  for (auto req : requesters_) {
    if (req) {
      delete req;
    }
  }
}

bool StunProber::Start(const std::vector<rtc::SocketAddress>& servers,
                       bool shared_socket_mode,
                       int interval_ms,
                       int num_request_per_ip,
                       int timeout_ms,
                       const AsyncCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  interval_ms_ = interval_ms;
  shared_socket_mode_ = shared_socket_mode;

  requests_per_ip_ = num_request_per_ip;
  if (requests_per_ip_ == 0 || servers.size() == 0) {
    return false;
  }

  timeout_ms_ = timeout_ms;
  servers_ = servers;
  finished_callback_ = callback;
  resolver_->Resolve(servers_[0], &resolved_ips_,
                     [this](int result) { this->OnServerResolved(0, result); });
  return true;
}

void StunProber::OnServerResolved(int index, int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result == 0) {
    all_servers_ips_.insert(all_servers_ips_.end(), resolved_ips_.begin(),
                            resolved_ips_.end());
    resolved_ips_.clear();
  }

  index++;

  if (static_cast<size_t>(index) < servers_.size()) {
    resolver_->Resolve(
        servers_[index], &resolved_ips_,
        [this, index](int result) { this->OnServerResolved(index, result); });
    return;
  }

  if (all_servers_ips_.size() == 0) {
    End(RESOLVE_FAILED, result);
    return;
  }

  // Dedupe.
  std::set<rtc::SocketAddress> addrs(all_servers_ips_.begin(),
                                     all_servers_ips_.end());
  all_servers_ips_.assign(addrs.begin(), addrs.end());

  rtc::IPAddress addr;
  if (GetLocalAddress(&addr) != 0) {
    End(GENERIC_FAILURE, result);
    return;
  }

  socket_factory_->Prepare(GetTotalClientSockets(), GetTotalServerSockets(),
                           [this](int result) {
                             if (result == 0) {
                               this->MaybeScheduleStunRequests();
                             }
                           });
}

int StunProber::GetLocalAddress(rtc::IPAddress* addr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (local_addr_.family() == AF_UNSPEC) {
    rtc::SocketAddress sock_addr;
    rtc::scoped_ptr<ClientSocketInterface> socket(
        socket_factory_->CreateClientSocket());
    int rv = socket->Connect(all_servers_ips_[0]);
    if (rv != SUCCESS) {
      End(GENERIC_FAILURE, rv);
      return rv;
    }
    rv = socket->GetLocalAddress(&sock_addr);
    if (rv != SUCCESS) {
      End(GENERIC_FAILURE, rv);
      return rv;
    }
    local_addr_ = sock_addr.ipaddr();
    socket->Close();
  }
  *addr = local_addr_;
  return 0;
}

StunProber::Requester* StunProber::CreateRequester() {
  DCHECK(thread_checker_.CalledOnValidThread());
  rtc::scoped_ptr<ServerSocketInterface> socket(
      socket_factory_->CreateServerSocket(kMaxUdpBufferSize,
                                          kMaxUdpBufferSize));
  if (!socket) {
    return nullptr;
  }
  if (shared_socket_mode_) {
    return new Requester(this, socket.release(), all_servers_ips_);
  } else {
    std::vector<rtc::SocketAddress> server_ip;
    server_ip.push_back(
        all_servers_ips_[(num_request_sent_ % all_servers_ips_.size())]);
    return new Requester(this, socket.release(), server_ip);
  }
}

bool StunProber::SendNextRequest() {
  if (!current_requester_ || current_requester_->Done()) {
    current_requester_ = CreateRequester();
    requesters_.push_back(current_requester_);
  }
  if (!current_requester_) {
    return false;
  }
  current_requester_->SendStunRequest();
  num_request_sent_++;
  return true;
}

void StunProber::MaybeScheduleStunRequests() {
  DCHECK(thread_checker_.CalledOnValidThread());
  uint32 now = rtc::Time();

  if (Done()) {
    task_runner_->PostTask(rtc::Bind(&StunProber::End, this, SUCCESS, 0),
                           timeout_ms_);
    return;
  }
  if (now >= next_request_time_ms_) {
    if (!SendNextRequest()) {
      End(GENERIC_FAILURE, 0);
      return;
    }
    next_request_time_ms_ = now + interval_ms_;
  }
  task_runner_->PostTask(
      rtc::Bind(&StunProber::MaybeScheduleStunRequests, this), 1 /* ms */);
}

bool StunProber::GetStats(StunProber::Stats* prob_stats) {
  // No need to be on the same thread.
  if (!prob_stats) {
    return false;
  }

  StunProber::Stats stats;

  int rtt_sum = 0;
  bool behind_nat_set = false;
  int64 first_sent_time = 0;
  int64 last_sent_time = 0;

  // Track of how many srflx IP that we have seen.
  std::set<rtc::IPAddress> srflx_ips;

  // If we're not receiving any response on a given IP, all requests sent to
  // that IP should be ignored as this could just be an DNS error.
  std::map<rtc::IPAddress, int> num_response_per_ip;
  std::map<rtc::IPAddress, int> num_request_per_ip;

  for (auto* requester : requesters_) {
    for (auto request : requester->requests()) {
      if (request->sent_time_ms <= 0) {
        continue;
      }

      IncrementCounterByAddress(&num_request_per_ip, request->server_addr);

      if (!first_sent_time) {
        first_sent_time = request->sent_time_ms;
      }
      last_sent_time = request->sent_time_ms;

      if (request->received_time_ms < request->sent_time_ms) {
        continue;
      }

      IncrementCounterByAddress(&num_response_per_ip, request->server_addr);

      rtt_sum += request->rtt();
      if (!behind_nat_set) {
        stats.behind_nat = request->behind_nat;
        behind_nat_set = true;
      } else if (stats.behind_nat != request->behind_nat) {
        // Detect the inconsistency in NAT presence.
        return false;
      }
      stats.srflx_addrs.insert(request->srflx_addr.ToString());
      srflx_ips.insert(request->srflx_addr.ipaddr());
    }
  }

  // We're probably not behind a regular NAT. We have more than 1 distinct
  // server reflexive IPs.
  if (srflx_ips.size() > 1) {
    return false;
  }

  int num_sent = 0;
  int num_received = 0;
  int num_server_ip_with_response = 0;

  for (const auto& kv : num_response_per_ip) {
    DCHECK_GT(kv.second, 0);
    num_server_ip_with_response++;
    num_received += kv.second;
    num_sent += num_request_per_ip[kv.first];
  }

  // Not receiving any response, the trial is inconclusive.
  if (!num_received) {
    return false;
  }

  // Shared mode is only true if we use the shared socket and there are more
  // than 1 responding servers.
  stats.shared_socket_mode =
      shared_socket_mode_ && (num_server_ip_with_response > 1);

  stats.host_ip = local_addr_.ToString();
  stats.num_request_sent = num_sent;
  stats.num_response_received = num_received;
  stats.target_request_interval_ns = interval_ms_ * 1000;
  stats.symmetric_nat =
      stats.srflx_addrs.size() > static_cast<size_t>(GetTotalServerSockets());

  if (num_sent) {
    stats.success_percent = static_cast<int>(100 * num_received / num_sent);
  }

  if (num_sent > 1) {
    stats.actual_request_interval_ns =
        (1000 * (last_sent_time - first_sent_time)) / (num_sent - 1);
  }

  if (num_received) {
    stats.average_rtt_ms = static_cast<int>((rtt_sum / num_received));
  }

  *prob_stats = stats;
  return true;
}

void StunProber::End(StunProber::Status status, int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!finished_callback_.empty()) {
    AsyncCallback callback = finished_callback_;
    finished_callback_ = AsyncCallback();

    // Callback at the last since the prober might be deleted in the callback.
    callback(status);
  }
}

}  // namespace stunprober
