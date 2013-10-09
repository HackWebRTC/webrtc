/*
 * libjingle
 * Copyright 2012, Google, Inc.
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

#ifndef TALK_P2P_BASE_DTLSTRANSPORT_H_
#define TALK_P2P_BASE_DTLSTRANSPORT_H_

#include "talk/p2p/base/dtlstransportchannel.h"
#include "talk/p2p/base/transport.h"

namespace talk_base {
class SSLIdentity;
}

namespace cricket {

class PortAllocator;

// Base should be a descendant of cricket::Transport
template<class Base>
class DtlsTransport : public Base {
 public:
  DtlsTransport(talk_base::Thread* signaling_thread,
                talk_base::Thread* worker_thread,
                const std::string& content_name,
                PortAllocator* allocator,
                talk_base::SSLIdentity* identity)
      : Base(signaling_thread, worker_thread, content_name, allocator),
        identity_(identity) {
  }

  ~DtlsTransport() {
    Base::DestroyAllChannels();
  }
  virtual void SetIdentity_w(talk_base::SSLIdentity* identity) {
    identity_ = identity;
  }
  virtual bool GetIdentity_w(talk_base::SSLIdentity** identity) {
    if (!identity_)
      return false;

    *identity = identity_->GetReference();
    return true;
  }

  virtual bool ApplyLocalTransportDescription_w(TransportChannelImpl*
                                                channel) {
    talk_base::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();

    if (local_fp) {
      // Sanity check local fingerprint.
      if (identity_) {
        talk_base::scoped_ptr<talk_base::SSLFingerprint> local_fp_tmp(
            talk_base::SSLFingerprint::Create(local_fp->algorithm,
                                              identity_));
        ASSERT(local_fp_tmp.get() != NULL);
        if (!(*local_fp_tmp == *local_fp)) {
          LOG(LS_WARNING) << "Local fingerprint does not match identity";
          return false;
        }
      } else {
        LOG(LS_WARNING)
            << "Local fingerprint provided but no identity available";
        return false;
      }
    } else {
      identity_ = NULL;
    }

    if (!channel->SetLocalIdentity(identity_))
      return false;

    // Apply the description in the base class.
    return Base::ApplyLocalTransportDescription_w(channel);
  }

  virtual bool NegotiateTransportDescription_w(ContentAction local_role) {
    if (!Base::local_description() || !Base::remote_description()) {
      LOG(LS_INFO) << "Local and Remote description must be set before "
                   << "transport descriptions are negotiated";
      return false;
    }

    talk_base::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();
    talk_base::SSLFingerprint* remote_fp =
        Base::remote_description()->identity_fingerprint.get();

    if (remote_fp && local_fp) {
      remote_fingerprint_.reset(new talk_base::SSLFingerprint(*remote_fp));

      // From RFC 4145, section-4.1, The following are the values that the
      // 'setup' attribute can take in an offer/answer exchange:
      //       Offer      Answer
      //      ________________
      //      active     passive / holdconn
      //      passive    active / holdconn
      //      actpass    active / passive / holdconn
      //      holdconn   holdconn
      //
      // Set the role that is most conformant with RFC 5763, Section 5, bullet 1
      // The endpoint MUST use the setup attribute defined in [RFC4145].
      // The endpoint that is the offerer MUST use the setup attribute
      // value of setup:actpass and be prepared to receive a client_hello
      // before it receives the answer.  The answerer MUST use either a
      // setup attribute value of setup:active or setup:passive.  Note that
      // if the answerer uses setup:passive, then the DTLS handshake will
      // not begin until the answerer is received, which adds additional
      // latency. setup:active allows the answer and the DTLS handshake to
      // occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
      // party is active MUST initiate a DTLS handshake by sending a
      // ClientHello over each flow (host/port quartet).
      // IOW - actpass and passive modes should be treated as server and
      // active as client.
      ConnectionRole local_connection_role =
          Base::local_description()->connection_role;
      ConnectionRole remote_connection_role =
          Base::remote_description()->connection_role;

      bool is_remote_server = false;
      if (local_role == CA_OFFER) {
        if (local_connection_role != CONNECTIONROLE_ACTPASS) {
          LOG(LS_ERROR) << "Offerer must use actpass value for setup attribute";
          return false;
        }

        if (remote_connection_role == CONNECTIONROLE_ACTIVE ||
            remote_connection_role == CONNECTIONROLE_PASSIVE ||
            remote_connection_role == CONNECTIONROLE_NONE) {
          is_remote_server = (remote_connection_role == CONNECTIONROLE_PASSIVE);
        } else {
          LOG(LS_ERROR) << "Answerer must use either active or passive value "
                        << "for setup attribute";
          return false;
        }
        // If remote is NONE or ACTIVE it will act as client.
      } else {
        if (remote_connection_role != CONNECTIONROLE_ACTPASS &&
            remote_connection_role != CONNECTIONROLE_NONE) {
          LOG(LS_ERROR) << "Offerer must use actpass value for setup attribute";
          return false;
        }

        if (local_connection_role == CONNECTIONROLE_ACTIVE ||
            local_connection_role == CONNECTIONROLE_PASSIVE) {
          is_remote_server = (local_connection_role == CONNECTIONROLE_ACTIVE);
        } else {
          LOG(LS_ERROR) << "Answerer must use either active or passive value "
                        << "for setup attribute";
          return false;
        }

        // If local is passive, local will act as server.
      }

      secure_role_ = is_remote_server ? talk_base::SSL_CLIENT :
                                        talk_base::SSL_SERVER;

    } else if (local_fp && (local_role == CA_ANSWER)) {
      LOG(LS_ERROR)
          << "Local fingerprint supplied when caller didn't offer DTLS";
      return false;
    } else {
      // We are not doing DTLS
      remote_fingerprint_.reset(new talk_base::SSLFingerprint(
          "", NULL, 0));
    }

    // Now run the negotiation for the base class.
    return Base::NegotiateTransportDescription_w(local_role);
  }

  virtual DtlsTransportChannelWrapper* CreateTransportChannel(int component) {
    return new DtlsTransportChannelWrapper(
        this, Base::CreateTransportChannel(component));
  }

  virtual void DestroyTransportChannel(TransportChannelImpl* channel) {
    // Kind of ugly, but this lets us do the exact inverse of the create.
    DtlsTransportChannelWrapper* dtls_channel =
        static_cast<DtlsTransportChannelWrapper*>(channel);
    TransportChannelImpl* base_channel = dtls_channel->channel();
    delete dtls_channel;
    Base::DestroyTransportChannel(base_channel);
  }

  virtual bool GetSslRole_w(talk_base::SSLRole* ssl_role) const {
    ASSERT(ssl_role != NULL);
    *ssl_role = secure_role_;
    return true;
  }

 private:
  virtual bool ApplyNegotiatedTransportDescription_w(
      TransportChannelImpl* channel) {
    // Set ssl role. Role must be set before fingerprint is applied, which
    // initiates DTLS setup.
    if (!channel->SetSslRole(secure_role_)) {
      LOG(LS_INFO) << "Failed to set ssl role for the channel.";
      return false;
    }
    // Apply remote fingerprint.
    if (!channel->SetRemoteFingerprint(
        remote_fingerprint_->algorithm,
        reinterpret_cast<const uint8 *>(remote_fingerprint_->
                                    digest.data()),
        remote_fingerprint_->digest.length())) {
      return false;
    }
    return Base::ApplyNegotiatedTransportDescription_w(channel);
  }

  talk_base::SSLIdentity* identity_;
  talk_base::SSLRole secure_role_;
  talk_base::scoped_ptr<talk_base::SSLFingerprint> remote_fingerprint_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_DTLSTRANSPORT_H_
