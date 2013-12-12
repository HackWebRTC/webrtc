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

#ifndef TALK_P2P_BASE_TRANSPORTCHANNELPROXY_H_
#define TALK_P2P_BASE_TRANSPORTCHANNELPROXY_H_

#include <string>
#include <utility>
#include <vector>

#include "talk/base/messagehandler.h"
#include "talk/p2p/base/transportchannel.h"

namespace talk_base {
class Thread;
}

namespace cricket {

class TransportChannelImpl;

// Proxies calls between the client and the transport channel implementation.
// This is needed because clients are allowed to create channels before the
// network negotiation is complete.  Hence, we create a proxy up front, and
// when negotiation completes, connect the proxy to the implementaiton.
class TransportChannelProxy : public TransportChannel,
                              public talk_base::MessageHandler {
 public:
  TransportChannelProxy(const std::string& content_name,
                        const std::string& name,
                        int component);
  virtual ~TransportChannelProxy();

  const std::string& name() const { return name_; }
  TransportChannelImpl* impl() { return impl_; }

  // Sets the implementation to which we will proxy.
  void SetImplementation(TransportChannelImpl* impl);

  // Implementation of the TransportChannel interface.  These simply forward to
  // the implementation.
  virtual int SendPacket(const char* data, size_t len,
                         talk_base::DiffServCodePoint dscp,
                         int flags);
  virtual int SetOption(talk_base::Socket::Option opt, int value);
  virtual int GetError();
  virtual IceRole GetIceRole() const;
  virtual bool GetStats(ConnectionInfos* infos);
  virtual bool IsDtlsActive() const;
  virtual bool GetSslRole(talk_base::SSLRole* role) const;
  virtual bool SetSslRole(talk_base::SSLRole role);
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers);
  virtual bool GetSrtpCipher(std::string* cipher);
  virtual bool GetLocalIdentity(talk_base::SSLIdentity** identity) const;
  virtual bool GetRemoteCertificate(talk_base::SSLCertificate** cert) const;
  virtual bool ExportKeyingMaterial(const std::string& label,
                            const uint8* context,
                            size_t context_len,
                            bool use_context,
                            uint8* result,
                            size_t result_len);

 private:
  // Catch signals from the implementation channel.  These just forward to the
  // client (after updating our state to match).
  void OnReadableState(TransportChannel* channel);
  void OnWritableState(TransportChannel* channel);
  void OnReadPacket(TransportChannel* channel, const char* data, size_t size,
                    int flags);
  void OnReadyToSend(TransportChannel* channel);
  void OnRouteChange(TransportChannel* channel, const Candidate& candidate);

  void OnMessage(talk_base::Message* message);

  typedef std::pair<talk_base::Socket::Option, int> OptionPair;
  typedef std::vector<OptionPair> OptionList;
  std::string name_;
  talk_base::Thread* worker_thread_;
  TransportChannelImpl* impl_;
  OptionList pending_options_;
  std::vector<std::string> pending_srtp_ciphers_;

  DISALLOW_EVIL_CONSTRUCTORS(TransportChannelProxy);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTCHANNELPROXY_H_
