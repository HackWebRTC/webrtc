/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>

#include "webrtc/base/checks.h"
#include "webrtc/base/flags.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
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

DEFINE_bool(help, false, "Prints this message");
DEFINE_int(interval, 10, "Interval of consecutive stun pings in milliseconds");
DEFINE_bool(shared_socket, false, "Share socket mode for different remote IPs");
DEFINE_int(pings_per_ip,
           10,
           "Number of consecutive stun pings to send for each IP");
DEFINE_int(timeout,
           1000,
           "Milliseconds of wait after the last ping sent before exiting");
DEFINE_int(port, 3478, "STUN server port");
DEFINE_string(server, "stun.voxgratia.org", "STUN server address");

namespace {

class HostNameResolver : public HostNameResolverInterface,
                         public sigslot::has_slots<> {
 public:
  HostNameResolver() { resolver_ = new rtc::AsyncResolver(); }
  virtual ~HostNameResolver() {
    // rtc::AsyncResolver inherits from SignalThread which requires explicit
    // Release().
    resolver_->Release();
  }

  void Resolve(const rtc::SocketAddress& addr,
               std::vector<rtc::IPAddress>* addresses,
               AsyncCallback callback) override {
    DCHECK(callback_.empty());
    addr_ = addr;
    callback_ = callback;
    result_ = addresses;
    resolver_->SignalDone.connect(this, &HostNameResolver::OnResolveResult);
    resolver_->Start(addr);
  }

  void OnResolveResult(rtc::AsyncResolverInterface* resolver) {
    DCHECK(resolver);
    int rv = resolver_->GetError();
    LOG(LS_INFO) << "ResolveResult for " << addr_.ToString() << " : " << rv;
    if (rv == 0 && result_) {
      *result_ = resolver_->addresses();

      for (auto& ip : *result_) {
        LOG(LS_INFO) << "\t" << ip.ToString();
      }
    }
    if (!callback_.empty()) {
      // Need to be the last statement as the object could be deleted by the
      // callback_ in the failure case.
      callback_(rv);
    }
  }

 private:
  AsyncCallback callback_;
  rtc::SocketAddress addr_;
  std::vector<rtc::IPAddress>* result_;

  // Not using smart ptr here as this requires specific release pattern.
  rtc::AsyncResolver* resolver_;
};

std::string HistogramName(bool behind_nat,
                          bool is_src_port_shared,
                          int interval_ms,
                          std::string suffix) {
  char output[1000];
  rtc::sprintfn(output, sizeof(output), "NetConnectivity6.%s.%s.%dms.%s",
                behind_nat ? "NAT" : "NoNAT",
                is_src_port_shared ? "SrcPortShared" : "SrcPortUnique",
                interval_ms, suffix.c_str());
  return std::string(output);
}

void PrintStats(StunProber* prober) {
  StunProber::Stats stats;
  if (!prober->GetStats(&stats)) {
    LOG(LS_WARNING) << "Results are inconclusive.";
    return;
  }

  LOG(LS_INFO) << "Requests sent: " << stats.num_request_sent;
  LOG(LS_INFO) << "Responses received: " << stats.num_response_received;
  LOG(LS_INFO) << "Target interval (ns): " << stats.target_request_interval_ns;
  LOG(LS_INFO) << "Actual interval (ns): " << stats.actual_request_interval_ns;
  LOG(LS_INFO) << "Behind NAT: " << stats.behind_nat;
  if (stats.behind_nat) {
    LOG(LS_INFO) << "NAT is symmetrical: " << (stats.srflx_ips.size() > 1);
  }
  LOG(LS_INFO) << "Host IP: " << stats.host_ip;
  LOG(LS_INFO) << "Server-reflexive ips: ";
  for (auto& ip : stats.srflx_ips) {
    LOG(LS_INFO) << "\t" << ip;
  }

  std::string histogram_name = HistogramName(
      stats.behind_nat, FLAG_shared_socket, FLAG_interval, "SuccessPercent");

  LOG(LS_INFO) << "Histogram '" << histogram_name.c_str()
               << "' = " << stats.success_percent;

  histogram_name = HistogramName(stats.behind_nat, FLAG_shared_socket,
                                 FLAG_interval, "ResponseLatency");

  LOG(LS_INFO) << "Histogram '" << histogram_name.c_str()
               << "' = " << stats.average_rtt_ms << " ms";
}

void StopTrial(rtc::Thread* thread, StunProber* prober, int result) {
  thread->Quit();
  if (prober) {
    LOG(LS_INFO) << "Result: " << result;
    if (result == StunProber::SUCCESS) {
      PrintStats(prober);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((FLAG_port < 1) || (FLAG_port > 65535)) {
    printf("Error: %i is not a valid port.\n", FLAG_port);
    return -1;
  }

  rtc::InitializeSSL();
  rtc::InitRandom(rtc::Time());
  rtc::Thread* thread = rtc::ThreadManager::Instance()->WrapCurrentThread();
  StunProber* prober = new StunProber(new HostNameResolver(),
                                      new SocketFactory(), new TaskRunner());
  auto finish_callback =
      [thread, prober](int result) { StopTrial(thread, prober, result); };
  prober->Start(FLAG_server, FLAG_port, FLAG_shared_socket, FLAG_interval,
                FLAG_pings_per_ip, FLAG_timeout,
                AsyncCallback(finish_callback));
  thread->Run();
  delete prober;
  return 0;
}
