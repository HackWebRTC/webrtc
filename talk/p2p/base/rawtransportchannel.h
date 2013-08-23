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

#ifndef TALK_P2P_BASE_RAWTRANSPORTCHANNEL_H_
#define TALK_P2P_BASE_RAWTRANSPORTCHANNEL_H_

#include <string>
#include <vector>
#include "talk/base/messagequeue.h"
#include "talk/p2p/base/transportchannelimpl.h"
#include "talk/p2p/base/rawtransport.h"
#include "talk/p2p/base/candidate.h"

#if defined(FEATURE_ENABLE_PSTN)

namespace talk_base {
class Thread;
}

namespace cricket {

class Connection;
class PortAllocator;
class PortAllocatorSession;
class PortInterface;
class RelayPort;
class StunPort;

// Implements a channel that just sends bare packets once we have received the
// address of the other side.  We pick a single address to send them based on
// a simple investigation of NAT type.
class RawTransportChannel : public TransportChannelImpl,
    public talk_base::MessageHandler {
 public:
  RawTransportChannel(const std::string& content_name,
                      int component,
                      RawTransport* transport,
                      talk_base::Thread *worker_thread,
                      PortAllocator *allocator);
  virtual ~RawTransportChannel();

  // Implementation of normal channel packet sending.
  virtual int SendPacket(const char *data, size_t len, int flags);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError();

  // Implements TransportChannelImpl.
  virtual Transport* GetTransport() { return raw_transport_; }
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd) {}
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd) {}

  // Creates an allocator session to start figuring out which type of
  // port we should send to the other client.  This will send
  // SignalAvailableCandidate once we have decided.
  virtual void Connect();

  // Resets state back to unconnected.
  virtual void Reset();

  // We don't actually worry about signaling since we can't send new candidates.
  virtual void OnSignalingReady() {}

  // Handles a message setting the remote address.  We are writable once we
  // have this since we now know where to send.
  virtual void OnCandidate(const Candidate& candidate);

  void OnRemoteAddress(const talk_base::SocketAddress& remote_address);

  // Below ICE specific virtual methods not implemented.
  virtual IceRole GetIceRole() const { return ICEROLE_UNKNOWN; }
  virtual void SetIceRole(IceRole role) {}
  virtual void SetIceTiebreaker(uint64 tiebreaker) {}
  virtual void SetIceProtocolType(IceProtocolType type) {}
  virtual void SetIceUfrag(const std::string& ice_ufrag) {}
  virtual void SetIcePwd(const std::string& ice_pwd) {}
  virtual void SetRemoteIceMode(IceMode mode) {}

  virtual bool GetStats(ConnectionInfos* infos) {
    return false;
  }

  // DTLS methods.
  virtual bool IsDtlsActive() const { return false; }

  // Default implementation.
  virtual bool GetSslRole(talk_base::SSLRole* role) const {
    return false;
  }

  virtual bool SetSslRole(talk_base::SSLRole role) {
    return false;
  }

  // Set up the ciphers to use for DTLS-SRTP.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) {
    return false;
  }

  // Find out which DTLS-SRTP cipher was negotiated
  virtual bool GetSrtpCipher(std::string* cipher) {
    return false;
  }

  // Allows key material to be extracted for external encryption.
  virtual bool ExportKeyingMaterial(
      const std::string& label,
      const uint8* context,
      size_t context_len,
      bool use_context,
      uint8* result,
      size_t result_len) {
    return false;
  }

  virtual bool SetLocalIdentity(talk_base::SSLIdentity* identity) {
    return false;
  }

  // Set DTLS Remote fingerprint. Must be after local identity set.
  virtual bool SetRemoteFingerprint(
    const std::string& digest_alg,
    const uint8* digest,
    size_t digest_len) {
    return false;
  }

 private:
  RawTransport* raw_transport_;
  talk_base::Thread *worker_thread_;
  PortAllocator* allocator_;
  PortAllocatorSession* allocator_session_;
  StunPort* stun_port_;
  RelayPort* relay_port_;
  PortInterface* port_;
  bool use_relay_;
  talk_base::SocketAddress remote_address_;

  // Called when the allocator creates another port.
  void OnPortReady(PortAllocatorSession* session, PortInterface* port);

  // Called when one of the ports we are using has determined its address.
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);

  // Called once we have chosen the port to use for communication with the
  // other client.  This will send its address and prepare the port for use.
  void SetPort(PortInterface* port);

  // Called once we have a port and a remote address.  This will set mark the
  // channel as writable and signal the route to the client.
  void SetWritable();

  // Called when we receive a packet from the other client.
  void OnReadPacket(PortInterface* port, const char* data, size_t size,
                    const talk_base::SocketAddress& addr);

  // Handles a message to destroy unused ports.
  virtual void OnMessage(talk_base::Message *msg);

  DISALLOW_EVIL_CONSTRUCTORS(RawTransportChannel);
};

}  // namespace cricket

#endif  // defined(FEATURE_ENABLE_PSTN)
#endif  // TALK_P2P_BASE_RAWTRANSPORTCHANNEL_H_
