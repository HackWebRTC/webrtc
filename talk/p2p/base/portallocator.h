/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_P2P_BASE_PORTALLOCATOR_H_
#define TALK_P2P_BASE_PORTALLOCATOR_H_

#include <string>
#include <vector>

#include "talk/base/helpers.h"
#include "talk/base/proxyinfo.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/portinterface.h"

namespace cricket {

// PortAllocator is responsible for allocating Port types for a given
// P2PSocket. It also handles port freeing.
//
// Clients can override this class to control port allocation, including
// what kinds of ports are allocated.

const uint32 PORTALLOCATOR_DISABLE_UDP = 0x01;
const uint32 PORTALLOCATOR_DISABLE_STUN = 0x02;
const uint32 PORTALLOCATOR_DISABLE_RELAY = 0x04;
const uint32 PORTALLOCATOR_DISABLE_TCP = 0x08;
const uint32 PORTALLOCATOR_ENABLE_SHAKER = 0x10;
const uint32 PORTALLOCATOR_ENABLE_BUNDLE = 0x20;
const uint32 PORTALLOCATOR_ENABLE_IPV6 = 0x40;
const uint32 PORTALLOCATOR_ENABLE_SHARED_UFRAG = 0x80;
const uint32 PORTALLOCATOR_ENABLE_SHARED_SOCKET = 0x100;
const uint32 PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE = 0x200;
const uint32 PORTALLOCATOR_USE_LARGE_SOCKET_SEND_BUFFERS = 0x400;

const uint32 kDefaultPortAllocatorFlags = 0;

const uint32 kDefaultStepDelay = 1000;  // 1 sec step delay.
// As per RFC 5245 Appendix B.1, STUN transactions need to be paced at certain
// internal. Less than 20ms is not acceptable. We choose 50ms as our default.
const uint32 kMinimumStepDelay = 50;

class PortAllocatorSessionMuxer;

class PortAllocatorSession : public sigslot::has_slots<> {
 public:
  // Content name passed in mostly for logging and debugging.
  // TODO(mallinath) - Change username and password to ice_ufrag and ice_pwd.
  PortAllocatorSession(const std::string& content_name,
                       int component,
                       const std::string& username,
                       const std::string& password,
                       uint32 flags);

  // Subclasses should clean up any ports created.
  virtual ~PortAllocatorSession() {}

  uint32 flags() const { return flags_; }
  void set_flags(uint32 flags) { flags_ = flags; }
  std::string content_name() const { return content_name_; }
  int component() const { return component_; }

  // Starts gathering STUN and Relay configurations.
  virtual void StartGettingPorts() = 0;
  virtual void StopGettingPorts() = 0;
  virtual bool IsGettingPorts() = 0;

  sigslot::signal2<PortAllocatorSession*, PortInterface*> SignalPortReady;
  sigslot::signal2<PortAllocatorSession*,
                   const std::vector<Candidate>&> SignalCandidatesReady;
  sigslot::signal1<PortAllocatorSession*> SignalCandidatesAllocationDone;

  virtual uint32 generation() { return generation_; }
  virtual void set_generation(uint32 generation) { generation_ = generation; }
  sigslot::signal1<PortAllocatorSession*> SignalDestroyed;

 protected:
  const std::string& username() const { return username_; }
  const std::string& password() const { return password_; }

  std::string content_name_;
  int component_;

 private:
  uint32 flags_;
  uint32 generation_;
  std::string username_;
  std::string password_;
};

class PortAllocator : public sigslot::has_slots<> {
 public:
  PortAllocator() :
      flags_(kDefaultPortAllocatorFlags),
      min_port_(0),
      max_port_(0),
      step_delay_(kDefaultStepDelay) {
    // This will allow us to have old behavior on non webrtc clients.
  }
  virtual ~PortAllocator();

  PortAllocatorSession* CreateSession(
      const std::string& sid,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd);

  PortAllocatorSessionMuxer* GetSessionMuxer(const std::string& key) const;
  void OnSessionMuxerDestroyed(PortAllocatorSessionMuxer* session);

  uint32 flags() const { return flags_; }
  void set_flags(uint32 flags) { flags_ = flags; }

  const std::string& user_agent() const { return agent_; }
  const talk_base::ProxyInfo& proxy() const { return proxy_; }
  void set_proxy(const std::string& agent, const talk_base::ProxyInfo& proxy) {
    agent_ = agent;
    proxy_ = proxy;
  }

  // Gets/Sets the port range to use when choosing client ports.
  int min_port() const { return min_port_; }
  int max_port() const { return max_port_; }
  bool SetPortRange(int min_port, int max_port) {
    if (min_port > max_port) {
      return false;
    }

    min_port_ = min_port;
    max_port_ = max_port;
    return true;
  }

  void set_step_delay(uint32 delay) {
    ASSERT(delay >= kMinimumStepDelay);
    step_delay_ = delay;
  }
  uint32 step_delay() const { return step_delay_; }

 protected:
  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) = 0;

  typedef std::map<std::string, PortAllocatorSessionMuxer*> SessionMuxerMap;

  uint32 flags_;
  std::string agent_;
  talk_base::ProxyInfo proxy_;
  int min_port_;
  int max_port_;
  uint32 step_delay_;
  SessionMuxerMap muxers_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_PORTALLOCATOR_H_
