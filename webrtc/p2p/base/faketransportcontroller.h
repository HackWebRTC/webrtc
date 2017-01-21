/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_
#define WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/bind.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslfingerprint.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/candidatepairinterface.h"
#include "webrtc/p2p/base/dtlstransportinternal.h"
#include "webrtc/p2p/base/icetransportinternal.h"

#include "webrtc/p2p/base/transportcontroller.h"

#ifdef HAVE_QUIC
#include "webrtc/p2p/quic/quictransport.h"
#endif

namespace cricket {

namespace {
struct PacketMessageData : public rtc::MessageData {
  PacketMessageData(const char* data, size_t len) : packet(data, len) {}
  rtc::Buffer packet;
};
}  // namespace

class FakeIceTransport : public IceTransportInternal,
                         public rtc::MessageHandler {
 public:
  explicit FakeIceTransport(const std::string& name, int component)
      : name_(name), component_(component) {}
  ~FakeIceTransport() { Reset(); }

  const std::string& transport_name() const override { return name_; }
  int component() const override { return component_; }
  uint64_t IceTiebreaker() const { return tiebreaker_; }
  IceMode remote_ice_mode() const { return remote_ice_mode_; }
  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }
  const std::string& remote_ice_ufrag() const { return remote_ice_ufrag_; }
  const std::string& remote_ice_pwd() const { return remote_ice_pwd_; }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { async_ = async; }
  void SetAsyncDelay(int delay_ms) { async_delay_ms_ = delay_ms; }

  IceTransportState GetState() const override {
    if (connection_count_ == 0) {
      return had_connection_ ? IceTransportState::STATE_FAILED
                             : IceTransportState::STATE_INIT;
    }

    if (connection_count_ == 1) {
      return IceTransportState::STATE_COMPLETED;
    }

    return IceTransportState::STATE_CONNECTING;
  }

  void SetIceRole(IceRole role) override { role_ = role; }
  IceRole GetIceRole() const override { return role_; }
  void SetIceTiebreaker(uint64_t tiebreaker) override {
    tiebreaker_ = tiebreaker;
  }
  void SetIceParameters(const IceParameters& ice_params) override {
    ice_ufrag_ = ice_params.ufrag;
    ice_pwd_ = ice_params.pwd;
  }
  void SetRemoteIceParameters(const IceParameters& params) override {
    remote_ice_ufrag_ = params.ufrag;
    remote_ice_pwd_ = params.pwd;
  }

  void SetRemoteIceMode(IceMode mode) override { remote_ice_mode_ = mode; }

  void MaybeStartGathering() override {
    if (gathering_state_ == kIceGatheringNew) {
      gathering_state_ = kIceGatheringGathering;
      SignalGatheringState(this);
    }
  }

  IceGatheringState gathering_state() const override {
    return gathering_state_;
  }

  void Reset() {
    if (state_ != STATE_INIT) {
      state_ = STATE_INIT;
      if (dest_) {
        dest_->state_ = STATE_INIT;
        dest_->dest_ = nullptr;
        dest_ = nullptr;
      }
    }
  }

  void SetWritable(bool writable) { set_writable(writable); }

  void set_writable(bool writable) {
    if (writable_ == writable) {
      return;
    }
    LOG(INFO) << "set_writable from:" << writable_ << " to " << writable;
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }
  bool writable() const override { return writable_; }

  // Simulates the two transports connecting to each other.
  // If |asymmetric| is true this method only affects this FakeIceTransport.
  // If false, it affects |dest| as well.
  void SetDestination(FakeIceTransport* dest, bool asymmetric = false) {
    if (state_ == STATE_INIT && dest) {
      // This simulates the delivery of candidates.
      dest_ = dest;
      state_ = STATE_CONNECTED;
      set_writable(true);
      if (!asymmetric) {
        dest->SetDestination(this, true);
      }
    } else if (state_ == STATE_CONNECTED && !dest) {
      // Simulates loss of connectivity, by asymmetrically forgetting dest_.
      dest_ = nullptr;
      state_ = STATE_INIT;
      set_writable(false);
    }
  }

  void SetConnectionCount(size_t connection_count) {
    size_t old_connection_count = connection_count_;
    connection_count_ = connection_count;
    if (connection_count)
      had_connection_ = true;
    // In this fake transport channel, |connection_count_| determines the
    // transport channel state.
    if (connection_count_ < old_connection_count)
      SignalStateChanged(this);
  }

  void SetCandidatesGatheringComplete() {
    if (gathering_state_ != kIceGatheringComplete) {
      gathering_state_ = kIceGatheringComplete;
      SignalGatheringState(this);
    }
  }

  void SetReceiving(bool receiving) { set_receiving(receiving); }

  void set_receiving(bool receiving) {
    if (receiving_ == receiving) {
      return;
    }
    receiving_ = receiving;
    SignalReceivingState(this);
  }
  bool receiving() const override { return receiving_; }

  void SetIceConfig(const IceConfig& config) override { ice_config_ = config; }

  int receiving_timeout() const { return ice_config_.receiving_timeout; }
  bool gather_continually() const { return ice_config_.gather_continually(); }

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    if (state_ != STATE_CONNECTED) {
      return -1;
    }

    if (flags != PF_SRTP_BYPASS && flags != 0) {
      return -1;
    }

    PacketMessageData* packet = new PacketMessageData(data, len);
    if (async_) {
      if (async_delay_ms_) {
        rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, async_delay_ms_,
                                            this, 0, packet);
      } else {
        rtc::Thread::Current()->Post(RTC_FROM_HERE, this, 0, packet);
      }
    } else {
      rtc::Thread::Current()->Send(RTC_FROM_HERE, this, 0, packet);
    }
    rtc::SentPacket sent_packet(options.packet_id, rtc::TimeMillis());
    SignalSentPacket(this, sent_packet);
    return static_cast<int>(len);
  }
  int SetOption(rtc::Socket::Option opt, int value) override { return true; }
  bool GetOption(rtc::Socket::Option opt, int* value) override { return true; }
  int GetError() override { return 0; }

  void AddRemoteCandidate(const Candidate& candidate) override {
    remote_candidates_.push_back(candidate);
  }

  void RemoveRemoteCandidate(const Candidate& candidate) override {}

  const Candidates& remote_candidates() const { return remote_candidates_; }

  void OnMessage(rtc::Message* msg) override {
    PacketMessageData* data = static_cast<PacketMessageData*>(msg->pdata);
    dest_->SignalReadPacket(dest_, data->packet.data<char>(),
                            data->packet.size(), rtc::CreatePacketTime(0), 0);
    delete data;
  }

  bool GetStats(ConnectionInfos* infos) override {
    ConnectionInfo info;
    infos->clear();
    infos->push_back(info);
    return true;
  }

  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override {
  }

 private:
  std::string name_;
  int component_;
  enum State { STATE_INIT, STATE_CONNECTED };
  FakeIceTransport* dest_ = nullptr;
  State state_ = STATE_INIT;
  bool async_ = false;
  int async_delay_ms_ = 0;
  Candidates remote_candidates_;
  IceConfig ice_config_;
  IceRole role_ = ICEROLE_UNKNOWN;
  uint64_t tiebreaker_ = 0;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceMode remote_ice_mode_ = ICEMODE_FULL;
  size_t connection_count_ = 0;
  IceGatheringState gathering_state_ = kIceGatheringNew;
  bool had_connection_ = false;
  bool writable_ = false;
  bool receiving_ = false;
};

class FakeDtlsTransport : public DtlsTransportInternal {
 public:
  explicit FakeDtlsTransport(FakeIceTransport* ice_transport)
      : ice_transport_(ice_transport),
        transport_name_(ice_transport->transport_name()),
        component_(ice_transport->component()),
        dtls_fingerprint_("", nullptr, 0) {
    ice_transport_->SignalReadPacket.connect(
        this, &FakeDtlsTransport::OnIceTransportReadPacket);
  }

  // If this constructor is called, a new fake ice transport will be created,
  // and this FakeDtlsTransport will take the ownership.
  explicit FakeDtlsTransport(const std::string& name, int component)
      : owned_ice_transport_(new FakeIceTransport(name, component)),
        transport_name_(owned_ice_transport_->transport_name()),
        component_(owned_ice_transport_->component()),
        dtls_fingerprint_("", nullptr, 0) {
    ice_transport_ = owned_ice_transport_.get();
    ice_transport_->SignalReadPacket.connect(
        this, &FakeDtlsTransport::OnIceTransportReadPacket);
  }

  ~FakeDtlsTransport() override { Reset(); }

  uint64_t IceTiebreaker() const { return ice_transport_->IceTiebreaker(); }
  IceMode remote_ice_mode() const { return ice_transport_->remote_ice_mode(); }
  const std::string& ice_ufrag() const { return ice_transport_->ice_ufrag(); }
  const std::string& ice_pwd() const { return ice_transport_->ice_pwd(); }
  const std::string& remote_ice_ufrag() const {
    return ice_transport_->remote_ice_ufrag();
  }
  const std::string& remote_ice_pwd() const {
    return ice_transport_->remote_ice_pwd();
  }

  DtlsTransportState dtls_state() const override { return dtls_state_; }

  const std::string& transport_name() const override { return transport_name_; }

  int component() const override { return component_; }

  const rtc::SSLFingerprint& dtls_fingerprint() const {
    return dtls_fingerprint_;
  }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { ice_transport_->SetAsync(async); }
  void SetAsyncDelay(int delay_ms) { ice_transport_->SetAsyncDelay(delay_ms); }

  IceRole GetIceRole() const { return ice_transport_->GetIceRole(); }

  bool SetRemoteFingerprint(const std::string& alg,
                            const uint8_t* digest,
                            size_t digest_len) override {
    dtls_fingerprint_ = rtc::SSLFingerprint(alg, digest, digest_len);
    return true;
  }

  bool SetSslRole(rtc::SSLRole role) override {
    ssl_role_ = role;
    return true;
  }

  bool GetSslRole(rtc::SSLRole* role) const override {
    *role = ssl_role_;
    return true;
  }

  IceGatheringState gathering_state() const {
    return ice_transport_->gathering_state();
  }

  void Reset() {
    if (state_ != STATE_INIT) {
      state_ = STATE_INIT;
      if (dest_) {
        dest_->state_ = STATE_INIT;
        dest_->dest_ = nullptr;
        dest_ = nullptr;
      }
    }
  }

  void SetWritable(bool writable) { set_writable(writable); }

  // Simulates the two transport channels connecting to each other.
  // If |asymmetric| is true this method only affects this FakeDtlsTransport.
  // If false, it affects |dest| as well.
  void SetDestination(FakeDtlsTransport* dest, bool asymmetric = false) {
    if (state_ == STATE_INIT && dest) {
      // This simulates the delivery of candidates.
      dest_ = dest;
      if (local_cert_ && dest_->local_cert_) {
        do_dtls_ = true;
        NegotiateSrtpCiphers();
      }
      state_ = STATE_CONNECTED;
      SetWritable(true);
      if (!asymmetric) {
        dest->SetDestination(this, true);
      }
      ice_transport_->SetDestination(
          static_cast<FakeIceTransport*>(dest->ice_transport()), asymmetric);
    } else if (state_ == STATE_CONNECTED && !dest) {
      // Simulates loss of connectivity, by asymmetrically forgetting dest_.
      dest_ = nullptr;
      state_ = STATE_INIT;
      SetWritable(false);
      ice_transport_->SetDestination(nullptr, asymmetric);
    }
  }

  void SetConnectionCount(size_t connection_count) {
    ice_transport_->SetConnectionCount(connection_count);
  }

  void SetCandidatesGatheringComplete() {
    ice_transport_->SetCandidatesGatheringComplete();
  }

  void SetReceiving(bool receiving) {
    ice_transport_->SetReceiving(receiving);
    set_receiving(receiving);
  }

  int receiving_timeout() const { return ice_transport_->receiving_timeout(); }
  bool gather_continually() const {
    return ice_transport_->gather_continually();
  }

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    return ice_transport_->SendPacket(data, len, options, flags);
  }

  bool GetOption(rtc::Socket::Option opt, int* value) override { return true; }

  const Candidates& remote_candidates() const {
    return ice_transport_->remote_candidates();
  }

  void OnIceTransportReadPacket(PacketTransportInterface* ice_,
                                const char* data,
                                size_t len,
                                const rtc::PacketTime& time,
                                int flags) {
    SignalReadPacket(this, data, len, time, flags);
  }

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    local_cert_ = certificate;
    return true;
  }

  void SetRemoteSSLCertificate(rtc::FakeSSLCertificate* cert) {
    remote_cert_ = cert;
  }

  bool IsDtlsActive() const override { return do_dtls_; }

  bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) override {
    srtp_ciphers_ = ciphers;
    return true;
  }

  bool GetSrtpCryptoSuite(int* crypto_suite) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      *crypto_suite = chosen_crypto_suite_;
      return true;
    }
    return false;
  }

  bool GetSslCipherSuite(int* cipher_suite) override { return false; }

  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override {
    return local_cert_;
  }

  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const override {
    return remote_cert_ ? std::unique_ptr<rtc::SSLCertificate>(
                              remote_cert_->GetReference())
                        : nullptr;
  }

  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      memset(result, 0xff, result_len);
      return true;
    }

    return false;
  }

  void set_ssl_max_protocol_version(rtc::SSLProtocolVersion version) {
    ssl_max_version_ = version;
  }
  rtc::SSLProtocolVersion ssl_max_protocol_version() const {
    return ssl_max_version_;
  }

  IceTransportInternal* ice_transport() override { return ice_transport_; }

  bool writable() const override { return writable_; }

  bool receiving() const override { return receiving_; }

  int GetError() override { return ice_transport_->GetError(); }

  int SetOption(rtc::Socket::Option opt, int value) override {
    return ice_transport_->SetOption(opt, value);
  }

  bool SetSrtpCiphers(const std::vector<std::string>& ciphers) override {
    std::vector<int> crypto_suites;
    for (const auto cipher : ciphers) {
      crypto_suites.push_back(rtc::SrtpCryptoSuiteFromName(cipher));
    }
    return SetSrtpCryptoSuites(crypto_suites);
  }

 private:
  void NegotiateSrtpCiphers() {
    for (std::vector<int>::const_iterator it1 = srtp_ciphers_.begin();
         it1 != srtp_ciphers_.end(); ++it1) {
      for (std::vector<int>::const_iterator it2 = dest_->srtp_ciphers_.begin();
           it2 != dest_->srtp_ciphers_.end(); ++it2) {
        if (*it1 == *it2) {
          chosen_crypto_suite_ = *it1;
          return;
        }
      }
    }
  }

  void set_receiving(bool receiving) {
    if (receiving_ == receiving) {
      return;
    }
    receiving_ = receiving;
    SignalReceivingState(this);
  }

  void set_writable(bool writable) {
    if (writable_ == writable) {
      return;
    }
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }

  enum State { STATE_INIT, STATE_CONNECTED };
  FakeIceTransport* ice_transport_;
  std::unique_ptr<FakeIceTransport> owned_ice_transport_;
  std::string transport_name_;
  int component_;
  FakeDtlsTransport* dest_ = nullptr;
  State state_ = STATE_INIT;
  Candidates remote_candidates_;
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert_;
  rtc::FakeSSLCertificate* remote_cert_ = nullptr;
  bool do_dtls_ = false;
  std::vector<int> srtp_ciphers_;
  int chosen_crypto_suite_ = rtc::SRTP_INVALID_CRYPTO_SUITE;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  rtc::SSLFingerprint dtls_fingerprint_;
  rtc::SSLRole ssl_role_ = rtc::SSL_CLIENT;

  DtlsTransportState dtls_state_ = DTLS_TRANSPORT_NEW;

  bool receiving_ = false;
  bool writable_ = false;
};

// Fake candidate pair class, which can be passed to BaseChannel for testing
// purposes.
class FakeCandidatePair : public CandidatePairInterface {
 public:
  FakeCandidatePair(const Candidate& local_candidate,
                    const Candidate& remote_candidate)
      : local_candidate_(local_candidate),
        remote_candidate_(remote_candidate) {}
  const Candidate& local_candidate() const override { return local_candidate_; }
  const Candidate& remote_candidate() const override {
    return remote_candidate_;
  }

 private:
  Candidate local_candidate_;
  Candidate remote_candidate_;
};

// Fake TransportController class, which can be passed into a BaseChannel object
// for test purposes. Can be connected to other FakeTransportControllers via
// Connect().
//
// This fake is unusual in that for the most part, it's implemented with the
// real TransportController code, but with fake TransportChannels underneath.
class FakeTransportController : public TransportController {
 public:
  FakeTransportController()
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr) {}

  explicit FakeTransportController(bool redetermine_role_on_ice_restart)
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr,
                            redetermine_role_on_ice_restart) {}

  explicit FakeTransportController(IceRole role)
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr) {
    SetIceRole(role);
  }

  explicit FakeTransportController(rtc::Thread* network_thread)
      : TransportController(rtc::Thread::Current(), network_thread, nullptr) {}

  FakeTransportController(rtc::Thread* network_thread, IceRole role)
      : TransportController(rtc::Thread::Current(), network_thread, nullptr) {
    SetIceRole(role);
  }

  FakeDtlsTransport* GetFakeDtlsTransport_n(const std::string& transport_name,
                                            int component) {
    return static_cast<FakeDtlsTransport*>(
        get_channel_for_testing(transport_name, component));
  }

  // Simulate the exchange of transport descriptions, and the gathering and
  // exchange of ICE candidates.
  void Connect(FakeTransportController* dest) {
    for (const std::string& transport_name : transport_names_for_testing()) {
      TransportDescription local_desc(
          std::vector<std::string>(),
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH),
          rtc::CreateRandomString(cricket::ICE_PWD_LENGTH),
          cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_NONE,
          certificate_for_testing()
              ? rtc::SSLFingerprint::CreateFromCertificate(
                    certificate_for_testing())
              : nullptr);
      TransportDescription remote_desc(
          std::vector<std::string>(),
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH),
          rtc::CreateRandomString(cricket::ICE_PWD_LENGTH),
          cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_NONE,
          dest->certificate_for_testing()
              ? rtc::SSLFingerprint::CreateFromCertificate(
                    dest->certificate_for_testing())
              : nullptr);
      std::string err;
      SetLocalTransportDescription(transport_name, local_desc,
                                   cricket::CA_OFFER, &err);
      dest->SetRemoteTransportDescription(transport_name, local_desc,
                                          cricket::CA_OFFER, &err);
      dest->SetLocalTransportDescription(transport_name, remote_desc,
                                         cricket::CA_ANSWER, &err);
      SetRemoteTransportDescription(transport_name, remote_desc,
                                    cricket::CA_ANSWER, &err);
    }
    MaybeStartGathering();
    dest->MaybeStartGathering();
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&FakeTransportController::SetChannelDestinations_n, this,
                  dest));
  }

  FakeCandidatePair* CreateFakeCandidatePair(
      const rtc::SocketAddress& local_address,
      int16_t local_network_id,
      const rtc::SocketAddress& remote_address,
      int16_t remote_network_id) {
    Candidate local_candidate(0, "udp", local_address, 0u, "", "", "local", 0,
                              "foundation", local_network_id, 0);
    Candidate remote_candidate(0, "udp", remote_address, 0u, "", "", "local", 0,
                               "foundation", remote_network_id, 0);
    return new FakeCandidatePair(local_candidate, remote_candidate);
  }

  void DestroyRtcpTransport(const std::string& transport_name) {
    DestroyDtlsTransport_n(transport_name,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }

 protected:
  // The ICE channel is never actually used by TransportController directly,
  // since (currently) the DTLS channel pretends to be both ICE + DTLS. This
  // will change when we get rid of TransportChannelImpl.
  IceTransportInternal* CreateIceTransportChannel_n(
      const std::string& transport_name,
      int component) override {
    return new FakeIceTransport(transport_name, component);
  }

  DtlsTransportInternal* CreateDtlsTransportChannel_n(
      const std::string& transport_name,
      int component,
      IceTransportInternal* ice) override {
    return new FakeDtlsTransport(static_cast<FakeIceTransport*>(ice));
  }

 private:
  void SetChannelDestinations_n(FakeTransportController* dest) {
    for (DtlsTransportInternal* tc : channels_for_testing()) {
      FakeDtlsTransport* local = static_cast<FakeDtlsTransport*>(tc);
      FakeDtlsTransport* remote = dest->GetFakeDtlsTransport_n(
          local->transport_name(), local->component());
      if (remote) {
        bool asymmetric = false;
        local->SetDestination(remote, asymmetric);
      }
    }
  }
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_
