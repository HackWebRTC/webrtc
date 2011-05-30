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

#include "talk/p2p/base/transportchannelproxy.h"
#include "talk/base/common.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannelimpl.h"

namespace cricket {

TransportChannelProxy::TransportChannelProxy(const std::string& name,
                                             const std::string& content_type)
    : TransportChannel(name, content_type), impl_(NULL) {
}

TransportChannelProxy::~TransportChannelProxy() {
  if (impl_)
    impl_->GetTransport()->DestroyChannel(impl_->name());
}

void TransportChannelProxy::SetImplementation(TransportChannelImpl* impl) {
  impl_ = impl;
  impl_->SignalReadableState.connect(
      this, &TransportChannelProxy::OnReadableState);
  impl_->SignalWritableState.connect(
      this, &TransportChannelProxy::OnWritableState);
  impl_->SignalReadPacket.connect(this, &TransportChannelProxy::OnReadPacket);
  impl_->SignalRouteChange.connect(this, &TransportChannelProxy::OnRouteChange);
  for (OptionList::iterator it = pending_options_.begin();
       it != pending_options_.end();
       ++it) {
    impl_->SetOption(it->first, it->second);
  }
  pending_options_.clear();
}

int TransportChannelProxy::SendPacket(talk_base::Buffer* packet) {
  // Fail if we don't have an impl yet.
  return (impl_) ? impl_->SendPacket(packet) : -1;
}

int TransportChannelProxy::SendPacket(const char *data, size_t len) {
  // Fail if we don't have an impl yet.
  return (impl_) ? impl_->SendPacket(data, len) : -1;
}

int TransportChannelProxy::SetOption(talk_base::Socket::Option opt, int value) {
  if (impl_)
    return impl_->SetOption(opt, value);
  pending_options_.push_back(OptionPair(opt, value));
  return 0;
}

int TransportChannelProxy::GetError() {
  ASSERT(impl_ != NULL);  // should not be used until channel is writable
  return impl_->GetError();
}

P2PTransportChannel* TransportChannelProxy::GetP2PChannel() {
  if (impl_) {
      return impl_->GetP2PChannel();
  }
  return NULL;
}

void TransportChannelProxy::OnReadableState(TransportChannel* channel) {
  ASSERT(channel == impl_);
  set_readable(impl_->readable());
}

void TransportChannelProxy::OnWritableState(TransportChannel* channel) {
  ASSERT(channel == impl_);
  set_writable(impl_->writable());
}

void TransportChannelProxy::OnReadPacket(TransportChannel* channel,
                                         const char* data, size_t size) {
  ASSERT(channel == impl_);
  SignalReadPacket(this, data, size);
}

void TransportChannelProxy::OnRouteChange(TransportChannel* channel,
                                          const Candidate& candidate) {
  ASSERT(channel == impl_);
  SignalRouteChange(this, candidate);
}

}  // namespace cricket
