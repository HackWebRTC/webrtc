/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/p2p/base/portallocatorsessionproxy.h"

#include "talk/base/thread.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/portproxy.h"

namespace cricket {

enum {
  MSG_SEND_ALLOCATION_DONE = 1,
  MSG_SEND_ALLOCATED_PORTS,
};

typedef talk_base::TypedMessageData<PortAllocatorSessionProxy*> ProxyObjData;

PortAllocatorSessionMuxer::PortAllocatorSessionMuxer(
    PortAllocatorSession* session)
    : worker_thread_(talk_base::Thread::Current()),
      session_(session),
      candidate_done_signal_received_(false) {
  session_->SignalPortReady.connect(
      this, &PortAllocatorSessionMuxer::OnPortReady);
  session_->SignalCandidatesAllocationDone.connect(
      this, &PortAllocatorSessionMuxer::OnCandidatesAllocationDone);
}

PortAllocatorSessionMuxer::~PortAllocatorSessionMuxer() {
  for (size_t i = 0; i < session_proxies_.size(); ++i)
    delete session_proxies_[i];

  SignalDestroyed(this);
}

void PortAllocatorSessionMuxer::RegisterSessionProxy(
    PortAllocatorSessionProxy* session_proxy) {
  session_proxies_.push_back(session_proxy);
  session_proxy->SignalDestroyed.connect(
      this, &PortAllocatorSessionMuxer::OnSessionProxyDestroyed);
  session_proxy->set_impl(session_.get());

  // Populate new proxy session with the information available in the actual
  // implementation.
  if (!ports_.empty()) {
    worker_thread_->Post(
        this, MSG_SEND_ALLOCATED_PORTS, new ProxyObjData(session_proxy));
  }

  if (candidate_done_signal_received_) {
    worker_thread_->Post(
        this, MSG_SEND_ALLOCATION_DONE, new ProxyObjData(session_proxy));
  }
}

void PortAllocatorSessionMuxer::OnCandidatesAllocationDone(
    PortAllocatorSession* session) {
  candidate_done_signal_received_ = true;
}

void PortAllocatorSessionMuxer::OnPortReady(PortAllocatorSession* session,
                                            PortInterface* port) {
  ASSERT(session == session_.get());
  ports_.push_back(port);
  port->SignalDestroyed.connect(
      this, &PortAllocatorSessionMuxer::OnPortDestroyed);
}

void PortAllocatorSessionMuxer::OnPortDestroyed(PortInterface* port) {
  std::vector<PortInterface*>::iterator it =
      std::find(ports_.begin(), ports_.end(), port);
  if (it != ports_.end())
    ports_.erase(it);
}

void PortAllocatorSessionMuxer::OnSessionProxyDestroyed(
    PortAllocatorSession* proxy) {

  std::vector<PortAllocatorSessionProxy*>::iterator it =
      std::find(session_proxies_.begin(), session_proxies_.end(), proxy);
  if (it != session_proxies_.end()) {
    session_proxies_.erase(it);
  }

  if (session_proxies_.empty()) {
    // Destroy PortAllocatorSession and its associated muxer object if all
    // proxies belonging to this session are already destroyed.
    delete this;
  }
}

void PortAllocatorSessionMuxer::OnMessage(talk_base::Message *pmsg) {
  ProxyObjData* proxy = static_cast<ProxyObjData*>(pmsg->pdata);
  switch (pmsg->message_id) {
    case MSG_SEND_ALLOCATION_DONE:
      SendAllocationDone_w(proxy->data());
      delete proxy;
      break;
    case MSG_SEND_ALLOCATED_PORTS:
      SendAllocatedPorts_w(proxy->data());
      delete proxy;
      break;
    default:
      ASSERT(false);
      break;
  }
}

void PortAllocatorSessionMuxer::SendAllocationDone_w(
    PortAllocatorSessionProxy* proxy) {
  std::vector<PortAllocatorSessionProxy*>::iterator iter =
      std::find(session_proxies_.begin(), session_proxies_.end(), proxy);
  if (iter != session_proxies_.end()) {
    proxy->OnCandidatesAllocationDone(session_.get());
  }
}

void PortAllocatorSessionMuxer::SendAllocatedPorts_w(
    PortAllocatorSessionProxy* proxy) {
  std::vector<PortAllocatorSessionProxy*>::iterator iter =
      std::find(session_proxies_.begin(), session_proxies_.end(), proxy);
  if (iter != session_proxies_.end()) {
    for (size_t i = 0; i < ports_.size(); ++i) {
      PortInterface* port = ports_[i];
      proxy->OnPortReady(session_.get(), port);
      // If port already has candidates, send this to the clients of proxy
      // session. This can happen if proxy is created later than the actual
      // implementation.
      if (!port->Candidates().empty()) {
        proxy->OnCandidatesReady(session_.get(), port->Candidates());
      }
    }
  }
}

PortAllocatorSessionProxy::~PortAllocatorSessionProxy() {
  std::map<PortInterface*, PortProxy*>::iterator it;
  for (it = proxy_ports_.begin(); it != proxy_ports_.end(); it++)
    delete it->second;

  SignalDestroyed(this);
}

void PortAllocatorSessionProxy::set_impl(
    PortAllocatorSession* session) {
  impl_ = session;

  impl_->SignalCandidatesReady.connect(
      this, &PortAllocatorSessionProxy::OnCandidatesReady);
  impl_->SignalPortReady.connect(
      this, &PortAllocatorSessionProxy::OnPortReady);
  impl_->SignalCandidatesAllocationDone.connect(
      this, &PortAllocatorSessionProxy::OnCandidatesAllocationDone);
}

void PortAllocatorSessionProxy::StartGettingPorts() {
  ASSERT(impl_ != NULL);
  // Since all proxies share a common PortAllocatorSession, this check will
  // prohibit sending multiple STUN ping messages to the stun server, which
  // is a problem on Chrome. GetInitialPorts() and StartGetAllPorts() called
  // from the worker thread and are called together from TransportChannel,
  // checking for IsGettingAllPorts() for GetInitialPorts() will not be a
  // problem.
  if (!impl_->IsGettingPorts()) {
    impl_->StartGettingPorts();
  }
}

void PortAllocatorSessionProxy::StopGettingPorts() {
  ASSERT(impl_ != NULL);
  if (impl_->IsGettingPorts()) {
    impl_->StopGettingPorts();
  }
}

bool PortAllocatorSessionProxy::IsGettingPorts() {
  ASSERT(impl_ != NULL);
  return impl_->IsGettingPorts();
}

void PortAllocatorSessionProxy::OnPortReady(PortAllocatorSession* session,
                                            PortInterface* port) {
  ASSERT(session == impl_);

  PortProxy* proxy_port = new PortProxy();
  proxy_port->set_impl(port);
  proxy_ports_[port] = proxy_port;
  SignalPortReady(this, proxy_port);
}

void PortAllocatorSessionProxy::OnCandidatesReady(
    PortAllocatorSession* session,
    const std::vector<Candidate>& candidates) {
  ASSERT(session == impl_);

  // Since all proxy sessions share a common PortAllocatorSession,
  // all Candidates will have name associated with the common PAS.
  // Change Candidate name with the PortAllocatorSessionProxy name.
  std::vector<Candidate> our_candidates;
  for (size_t i = 0; i < candidates.size(); ++i) {
    Candidate new_local_candidate = candidates[i];
    new_local_candidate.set_component(component_);
    our_candidates.push_back(new_local_candidate);
  }
  SignalCandidatesReady(this, our_candidates);
}

void PortAllocatorSessionProxy::OnCandidatesAllocationDone(
    PortAllocatorSession* session) {
  ASSERT(session == impl_);
  SignalCandidatesAllocationDone(this);
}

}  // namespace cricket
