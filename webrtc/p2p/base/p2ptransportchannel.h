/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// P2PTransportChannel wraps up the state management of the connection between
// two P2P clients.  Clients have candidate ports for connecting, and
// connections which are combinations of candidates from each end (Alice and
// Bob each have candidates, one candidate from Alice and one candidate from
// Bob are used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_
#define WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_

#include <map>
#include <string>
#include <vector>
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

extern const uint32_t WEAK_PING_DELAY;

// Adds the port on which the candidate originated.
class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, PortInterface* origin_port)
      : Candidate(c), origin_port_(origin_port) {}

  PortInterface* origin_port() { return origin_port_; }

 private:
  PortInterface* origin_port_;
};

// P2PTransportChannel manages the candidates and connection process to keep
// two P2P clients connected to each other.
class P2PTransportChannel : public TransportChannelImpl,
                            public rtc::MessageHandler {
 public:
  P2PTransportChannel(const std::string& transport_name,
                      int component,
                      P2PTransport* transport,
                      PortAllocator* allocator);
  virtual ~P2PTransportChannel();

  // From TransportChannelImpl:
  Transport* GetTransport() override { return transport_; }
  TransportChannelState GetState() const override;
  void SetIceRole(IceRole role) override;
  IceRole GetIceRole() const override { return ice_role_; }
  void SetIceTiebreaker(uint64_t tiebreaker) override;
  void SetIceCredentials(const std::string& ice_ufrag,
                         const std::string& ice_pwd) override;
  void SetRemoteIceCredentials(const std::string& ice_ufrag,
                               const std::string& ice_pwd) override;
  void SetRemoteIceMode(IceMode mode) override;
  void Connect() override;
  void MaybeStartGathering() override;
  IceGatheringState gathering_state() const override {
    return gathering_state_;
  }
  void AddRemoteCandidate(const Candidate& candidate) override;
  // Sets the receiving timeout and gather_continually.
  // This also sets the check_receiving_delay proportionally.
  void SetIceConfig(const IceConfig& config) override;

  // From TransportChannel:
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  bool GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override { return error_; }
  bool GetStats(std::vector<ConnectionInfo>* stats) override;

  const Connection* best_connection() const { return best_connection_; }
  void set_incoming_only(bool value) { incoming_only_ = value; }

  // Note: This is only for testing purpose.
  // |ports_| should not be changed from outside.
  const std::vector<PortInterface*>& ports() { return ports_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

  // DTLS methods.
  bool IsDtlsActive() const override { return false; }

  // Default implementation.
  bool GetSslRole(rtc::SSLRole* role) const override { return false; }

  bool SetSslRole(rtc::SSLRole role) override { return false; }

  // Set up the ciphers to use for DTLS-SRTP.
  bool SetSrtpCiphers(const std::vector<std::string>& ciphers) override {
    return false;
  }

  // Find out which DTLS-SRTP cipher was negotiated.
  bool GetSrtpCryptoSuite(std::string* cipher) override { return false; }

  // Find out which DTLS cipher was negotiated.
  bool GetSslCipherSuite(int* cipher) override { return false; }

  // Returns null because the channel is not encrypted by default.
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override {
    return nullptr;
  }

  bool GetRemoteSSLCertificate(rtc::SSLCertificate** cert) const override {
    return false;
  }

  // Allows key material to be extracted for external encryption.
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override {
    return false;
  }

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    return false;
  }

  // Set DTLS Remote fingerprint. Must be after local identity set.
  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override {
    return false;
  }

  int receiving_timeout() const { return receiving_timeout_; }
  int check_receiving_delay() const { return check_receiving_delay_; }

  // Helper method used only in unittest.
  rtc::DiffServCodePoint DefaultDscpValue() const;

  // Public for unit tests.
  Connection* FindNextPingableConnection();

  // Public for unit tests.
  const std::vector<Connection*>& connections() const { return connections_; }

  // Public for unit tests.
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back();
  }

 private:
  rtc::Thread* thread() { return worker_thread_; }
  bool IsGettingPorts() { return allocator_session()->IsGettingPorts(); }

  // A transport channel is weak if the current best connection is either
  // not receiving or not writable, or if there is no best connection at all.
  bool weak() const;
  void UpdateConnectionStates();
  void RequestSort();
  void SortConnections();
  void SwitchBestConnectionTo(Connection* conn);
  void UpdateChannelState();
  void HandleWritable();
  void HandleNotWritable();
  void HandleAllTimedOut();
  void MaybeStopPortAllocatorSessions();

  Connection* GetBestConnectionOnNetwork(rtc::Network* network) const;
  bool CreateConnections(const Candidate& remote_candidate,
                         PortInterface* origin_port);
  bool CreateConnection(PortInterface* port,
                        const Candidate& remote_candidate,
                        PortInterface* origin_port);
  bool FindConnection(cricket::Connection* connection) const;

  uint32_t GetRemoteCandidateGeneration(const Candidate& candidate);
  bool IsDuplicateRemoteCandidate(const Candidate& candidate);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               PortInterface* origin_port);
  bool IsPingable(Connection* conn);
  void PingConnection(Connection* conn);
  void AddAllocatorSession(PortAllocatorSession* session);
  void AddConnection(Connection* connection);

  void OnPortReady(PortAllocatorSession *session, PortInterface* port);
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);
  void OnUnknownAddress(PortInterface* port,
                        const rtc::SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* stun_msg,
                        const std::string& remote_username,
                        bool port_muxed);
  void OnPortDestroyed(PortInterface* port);
  void OnRoleConflict(PortInterface* port);

  void OnConnectionStateChange(Connection* connection);
  void OnReadPacket(Connection *connection, const char *data, size_t len,
                    const rtc::PacketTime& packet_time);
  void OnSentPacket(PortInterface* port, const rtc::SentPacket& sent_packet);
  void OnReadyToSend(Connection* connection);
  void OnConnectionDestroyed(Connection *connection);

  void OnNominated(Connection* conn);

  void OnMessage(rtc::Message* pmsg) override;
  void OnSort();
  void OnCheckAndPing();

  void PruneConnections();
  Connection* best_nominated_connection() const;

  P2PTransport* transport_;
  PortAllocator* allocator_;
  rtc::Thread* worker_thread_;
  bool incoming_only_;
  int error_;
  std::vector<PortAllocatorSession*> allocator_sessions_;
  std::vector<PortInterface *> ports_;
  std::vector<Connection *> connections_;
  Connection* best_connection_;
  // Connection selected by the controlling agent. This should be used only
  // at controlled side when protocol type is RFC5245.
  Connection* pending_best_connection_;
  std::vector<RemoteCandidate> remote_candidates_;
  bool sort_dirty_;  // indicates whether another sort is needed right now
  bool had_connection_ = false;  // if connections_ has ever been nonempty
  typedef std::map<rtc::Socket::Option, int> OptionMap;
  OptionMap options_;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceMode remote_ice_mode_;
  IceRole ice_role_;
  uint64_t tiebreaker_;
  uint32_t remote_candidate_generation_;
  IceGatheringState gathering_state_;

  int check_receiving_delay_;
  int receiving_timeout_;
  uint32_t last_ping_sent_ms_ = 0;
  bool gather_continually_ = false;
  int weak_ping_delay_ = WEAK_PING_DELAY;

  RTC_DISALLOW_COPY_AND_ASSIGN(P2PTransportChannel);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_
