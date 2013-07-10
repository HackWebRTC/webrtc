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

#ifndef TALK_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_
#define TALK_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_

#include <string>

#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/portallocator.h"

namespace cricket {
class PortAllocator;
class PortAllocatorSessionProxy;
class PortProxy;

// This class maintains the list of cricket::Port* objects. Ports will be
// deleted upon receiving SignalDestroyed signal. This class is used when
// PORTALLOCATOR_ENABLE_BUNDLE flag is set.

class PortAllocatorSessionMuxer : public talk_base::MessageHandler,
                                  public sigslot::has_slots<> {
 public:
  explicit PortAllocatorSessionMuxer(PortAllocatorSession* session);
  virtual ~PortAllocatorSessionMuxer();

  void RegisterSessionProxy(PortAllocatorSessionProxy* session_proxy);

  void OnPortReady(PortAllocatorSession* session, PortInterface* port);
  void OnPortDestroyed(PortInterface* port);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);

  const std::vector<PortInterface*>& ports() { return ports_; }

  sigslot::signal1<PortAllocatorSessionMuxer*> SignalDestroyed;

 private:
  virtual void OnMessage(talk_base::Message *pmsg);
  void OnSessionProxyDestroyed(PortAllocatorSession* proxy);
  void SendAllocationDone_w(PortAllocatorSessionProxy* proxy);
  void SendAllocatedPorts_w(PortAllocatorSessionProxy* proxy);

  // Port will be deleted when SignalDestroyed received, otherwise delete
  // happens when PortAllocatorSession dtor is called.
  talk_base::Thread* worker_thread_;
  std::vector<PortInterface*> ports_;
  talk_base::scoped_ptr<PortAllocatorSession> session_;
  std::vector<PortAllocatorSessionProxy*> session_proxies_;
  bool candidate_done_signal_received_;
};

class PortAllocatorSessionProxy : public PortAllocatorSession {
 public:
  PortAllocatorSessionProxy(const std::string& content_name,
                            int component,
                            uint32 flags)
        // Use empty string as the ufrag and pwd because the proxy always uses
        // the ufrag and pwd from the underlying implementation.
      : PortAllocatorSession(content_name, component, "", "", flags),
        impl_(NULL) {
  }

  virtual ~PortAllocatorSessionProxy();

  PortAllocatorSession* impl() { return impl_; }
  void set_impl(PortAllocatorSession* session);

  // Forwards call to the actual PortAllocatorSession.
  virtual void StartGettingPorts();
  virtual void StopGettingPorts();
  virtual bool IsGettingPorts();

  virtual void set_generation(uint32 generation) {
    ASSERT(impl_ != NULL);
    impl_->set_generation(generation);
  }

  virtual uint32 generation() {
    ASSERT(impl_ != NULL);
    return impl_->generation();
  }

 private:
  void OnPortReady(PortAllocatorSession* session, PortInterface* port);
  void OnCandidatesReady(PortAllocatorSession* session,
                         const std::vector<Candidate>& candidates);
  void OnPortDestroyed(PortInterface* port);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);

  // This is the actual PortAllocatorSession, owned by PortAllocator.
  PortAllocatorSession* impl_;
  std::map<PortInterface*, PortProxy*> proxy_ports_;

  friend class PortAllocatorSessionMuxer;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_PORTALLOCATORSESSIONPROXY_H_
