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
    talk_base::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();
    talk_base::SSLFingerprint* remote_fp =
        Base::remote_description()->identity_fingerprint.get();

    if (remote_fp && local_fp) {
      remote_fingerprint_.reset(new talk_base::SSLFingerprint(*remote_fp));
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

 private:
  virtual void ApplyNegotiatedTransportDescription_w(
      TransportChannelImpl* channel) {
    channel->SetRemoteFingerprint(
        remote_fingerprint_->algorithm,
        reinterpret_cast<const uint8 *>(remote_fingerprint_->
                                    digest.data()),
        remote_fingerprint_->digest.length());
    Base::ApplyNegotiatedTransportDescription_w(channel);
  }

  talk_base::SSLIdentity* identity_;
  talk_base::scoped_ptr<talk_base::SSLFingerprint> remote_fingerprint_;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_DTLSTRANSPORT_H_
