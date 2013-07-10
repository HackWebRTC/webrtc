/*
 * libjingle
 * Copyright 2011, Google Inc.
 * Copyright 2011, RTFM, Inc.
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

#ifndef TALK_P2P_BASE_DTLSTRANSPORTCHANNEL_H_
#define TALK_P2P_BASE_DTLSTRANSPORTCHANNEL_H_

#include <string>
#include <vector>

#include "talk/base/buffer.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/stream.h"
#include "talk/p2p/base/transportchannelimpl.h"

namespace cricket {

// A bridge between a packet-oriented/channel-type interface on
// the bottom and a StreamInterface on the top.
class StreamInterfaceChannel : public talk_base::StreamInterface,
                               public sigslot::has_slots<> {
 public:
  StreamInterfaceChannel(talk_base::Thread* owner, TransportChannel* channel)
      : channel_(channel),
        state_(talk_base::SS_OPEN),
        fifo_(kFifoSize, owner) {
    fifo_.SignalEvent.connect(this, &StreamInterfaceChannel::OnEvent);
  }

  // Push in a packet; this gets pulled out from Read().
  bool OnPacketReceived(const char* data, size_t size);

  // Implementations of StreamInterface
  virtual talk_base::StreamState GetState() const { return state_; }
  virtual void Close() { state_ = talk_base::SS_CLOSED; }
  virtual talk_base::StreamResult Read(void* buffer, size_t buffer_len,
                                       size_t* read, int* error);
  virtual talk_base::StreamResult Write(const void* data, size_t data_len,
                                        size_t* written, int* error);

 private:
  static const size_t kFifoSize = 8192;

  // Forward events
  virtual void OnEvent(talk_base::StreamInterface* stream, int sig, int err);

  TransportChannel* channel_;  // owned by DtlsTransportChannelWrapper
  talk_base::StreamState state_;
  talk_base::FifoBuffer fifo_;

  DISALLOW_COPY_AND_ASSIGN(StreamInterfaceChannel);
};


// This class provides a DTLS SSLStreamAdapter inside a TransportChannel-style
// packet-based interface, wrapping an existing TransportChannel instance
// (e.g a P2PTransportChannel)
// Here's the way this works:
//
//   DtlsTransportChannelWrapper {
//       SSLStreamAdapter* dtls_ {
//           StreamInterfaceChannel downward_ {
//               TransportChannelImpl* channel_;
//           }
//       }
//   }
//
//   - Data which comes into DtlsTransportChannelWrapper from the underlying
//     channel_ via OnReadPacket() is checked for whether it is DTLS
//     or not, and if it is, is passed to DtlsTransportChannelWrapper::
//     HandleDtlsPacket, which pushes it into to downward_.
//     dtls_ is listening for events on downward_, so it immediately calls
//     downward_->Read().
//
//   - Data written to DtlsTransportChannelWrapper is passed either to
//      downward_ or directly to channel_, depending on whether DTLS is
//     negotiated and whether the flags include PF_SRTP_BYPASS
//
//   - The SSLStreamAdapter writes to downward_->Write()
//     which translates it into packet writes on channel_.
class DtlsTransportChannelWrapper : public TransportChannelImpl {
 public:
    enum State {
      STATE_NONE,      // No state or rejected.
      STATE_OFFERED,   // Our identity has been set.
      STATE_ACCEPTED,  // The other side sent a fingerprint.
      STATE_STARTED,   // We are negotiating.
      STATE_OPEN,      // Negotiation complete.
      STATE_CLOSED     // Connection closed.
    };

  // The parameters here are:
  // transport -- the DtlsTransport that created us
  // channel -- the TransportChannel we are wrapping
  DtlsTransportChannelWrapper(Transport* transport,
                              TransportChannelImpl* channel);
  virtual ~DtlsTransportChannelWrapper();

  virtual void SetRole(TransportRole role);
  // Returns current transport role of the channel.
  virtual TransportRole GetRole() const {
    return channel_->GetRole();
  }
  virtual bool SetLocalIdentity(talk_base::SSLIdentity *identity);

  virtual bool SetRemoteFingerprint(const std::string& digest_alg,
                                    const uint8* digest,
                                    size_t digest_len);
  virtual bool IsDtlsActive() const { return dtls_state_ != STATE_NONE; }

  // Called to send a packet (via DTLS, if turned on).
  virtual int SendPacket(const char* data, size_t size, int flags);

  // TransportChannel calls that we forward to the wrapped transport.
  virtual int SetOption(talk_base::Socket::Option opt, int value) {
    return channel_->SetOption(opt, value);
  }
  virtual int GetError() {
    return channel_->GetError();
  }
  virtual bool GetStats(ConnectionInfos* infos) {
    return channel_->GetStats(infos);
  }
  virtual void SetSessionId(const std::string& session_id) {
    channel_->SetSessionId(session_id);
  }
  virtual const std::string& SessionId() const {
    return channel_->SessionId();
  }

  // Set up the ciphers to use for DTLS-SRTP. If this method is not called
  // before DTLS starts, or |ciphers| is empty, SRTP keys won't be negotiated.
  // This method should be called before SetupDtls.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers);

  // Find out which DTLS-SRTP cipher was negotiated
  virtual bool GetSrtpCipher(std::string* cipher);

  // Once DTLS has established (i.e., this channel is writable), this method
  // extracts the keys negotiated during the DTLS handshake, for use in external
  // encryption. DTLS-SRTP uses this to extract the needed SRTP keys.
  // See the SSLStreamAdapter documentation for info on the specific parameters.
  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8* result,
                                    size_t result_len) {
    return (dtls_.get()) ? dtls_->ExportKeyingMaterial(label, context,
                                                       context_len,
                                                       use_context,
                                                       result, result_len)
        : false;
  }

  // TransportChannelImpl calls.
  virtual Transport* GetTransport() {
    return transport_;
  }
  virtual void SetTiebreaker(uint64 tiebreaker) {
    channel_->SetTiebreaker(tiebreaker);
  }
  virtual void SetIceProtocolType(IceProtocolType type) {
    channel_->SetIceProtocolType(type);
  }
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd) {
    channel_->SetIceCredentials(ice_ufrag, ice_pwd);
  }
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd) {
    channel_->SetRemoteIceCredentials(ice_ufrag, ice_pwd);
  }
  virtual void SetRemoteIceMode(IceMode mode) {
    channel_->SetRemoteIceMode(mode);
  }

  virtual void Connect();
  virtual void Reset();

  virtual void OnSignalingReady() {
    channel_->OnSignalingReady();
  }
  virtual void OnCandidate(const Candidate& candidate) {
    channel_->OnCandidate(candidate);
  }

  // Needed by DtlsTransport.
  TransportChannelImpl* channel() { return channel_; }

 private:
  void OnReadableState(TransportChannel* channel);
  void OnWritableState(TransportChannel* channel);
  void OnReadPacket(TransportChannel* channel, const char* data, size_t size,
                    int flags);
  void OnReadyToSend(TransportChannel* channel);
  void OnDtlsEvent(talk_base::StreamInterface* stream_, int sig, int err);
  bool SetupDtls();
  bool MaybeStartDtls();
  bool HandleDtlsPacket(const char* data, size_t size);
  void OnRequestSignaling(TransportChannelImpl* channel);
  void OnCandidateReady(TransportChannelImpl* channel, const Candidate& c);
  void OnCandidatesAllocationDone(TransportChannelImpl* channel);
  void OnRoleConflict(TransportChannelImpl* channel);
  void OnRouteChange(TransportChannel* channel, const Candidate& candidate);

  Transport* transport_;  // The transport_ that created us.
  talk_base::Thread* worker_thread_;  // Everything should occur on this thread.
  TransportChannelImpl* channel_;  // Underlying channel, owned by transport_.
  talk_base::scoped_ptr<talk_base::SSLStreamAdapter> dtls_;  // The DTLS stream
  StreamInterfaceChannel* downward_;  // Wrapper for channel_, owned by dtls_.
  std::vector<std::string> srtp_ciphers_;  // SRTP ciphers to use with DTLS.
  State dtls_state_;
  talk_base::SSLIdentity* local_identity_;
  talk_base::SSLRole dtls_role_;
  talk_base::Buffer remote_fingerprint_value_;
  std::string remote_fingerprint_algorithm_;

  DISALLOW_COPY_AND_ASSIGN(DtlsTransportChannelWrapper);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_DTLSTRANSPORTCHANNEL_H_
