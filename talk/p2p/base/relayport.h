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

#ifndef TALK_P2P_BASE_RELAYPORT_H_
#define TALK_P2P_BASE_RELAYPORT_H_

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "talk/p2p/base/port.h"
#include "talk/p2p/base/stunrequest.h"

namespace cricket {

class RelayEntry;
class RelayConnection;

// Communicates using an allocated port on the relay server. For each
// remote candidate that we try to send data to a RelayEntry instance
// is created. The RelayEntry will try to reach the remote destination
// by connecting to all available server addresses in a pre defined
// order with a small delay in between. When a connection is
// successful all other connection attemts are aborted.
class RelayPort : public Port {
 public:
  typedef std::pair<talk_base::Socket::Option, int> OptionValue;

  // RelayPort doesn't yet do anything fancy in the ctor.
  static RelayPort* Create(
      talk_base::Thread* thread, talk_base::PacketSocketFactory* factory,
      talk_base::Network* network, const talk_base::IPAddress& ip,
      int min_port, int max_port, const std::string& username,
      const std::string& password) {
    return new RelayPort(thread, factory, network, ip, min_port, max_port,
                         username, password);
  }
  virtual ~RelayPort();

  void AddServerAddress(const ProtocolAddress& addr);
  void AddExternalAddress(const ProtocolAddress& addr);

  const std::vector<OptionValue>& options() const { return options_; }
  bool HasMagicCookie(const char* data, size_t size);

  virtual void PrepareAddress();
  virtual Connection* CreateConnection(const Candidate& address,
                                       CandidateOrigin origin);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetOption(talk_base::Socket::Option opt, int* value);
  virtual int GetError();

  const ProtocolAddress * ServerAddress(size_t index) const;
  bool IsReady() { return ready_; }

  // Used for testing.
  sigslot::signal1<const ProtocolAddress*> SignalConnectFailure;
  sigslot::signal1<const ProtocolAddress*> SignalSoftTimeout;

 protected:
  RelayPort(talk_base::Thread* thread, talk_base::PacketSocketFactory* factory,
            talk_base::Network*, const talk_base::IPAddress& ip,
            int min_port, int max_port, const std::string& username,
            const std::string& password);
  bool Init();

  void SetReady();

  virtual int SendTo(const void* data, size_t size,
                     const talk_base::SocketAddress& addr,
                     talk_base::DiffServCodePoint dscp,
                     bool payload);

  // Dispatches the given packet to the port or connection as appropriate.
  void OnReadPacket(const char* data, size_t size,
                    const talk_base::SocketAddress& remote_addr,
                    ProtocolType proto);

 private:
  friend class RelayEntry;

  std::deque<ProtocolAddress> server_addr_;
  std::vector<ProtocolAddress> external_addr_;
  bool ready_;
  std::vector<RelayEntry*> entries_;
  std::vector<OptionValue> options_;
  int error_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_RELAYPORT_H_
