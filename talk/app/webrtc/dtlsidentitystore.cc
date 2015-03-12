/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#include "talk/app/webrtc/dtlsidentitystore.h"

#include "talk/app/webrtc/webrtcsessiondescriptionfactory.h"
#include "webrtc/base/logging.h"

using webrtc::DTLSIdentityRequestObserver;
using webrtc::WebRtcSessionDescriptionFactory;

namespace webrtc {

namespace {

enum {
  MSG_GENERATE_IDENTITY,
  MSG_GENERATE_IDENTITY_RESULT,
  MSG_RETURN_FREE_IDENTITY
};

typedef rtc::ScopedMessageData<rtc::SSLIdentity> IdentityResultMessageData;
}  // namespace

// Arbitrary constant used as common name for the identity.
// Chosen to make the certificates more readable.
const char DtlsIdentityStore::kIdentityName[] = "WebRTC";

DtlsIdentityStore::DtlsIdentityStore(rtc::Thread* signaling_thread,
                                     rtc::Thread* worker_thread)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      pending_jobs_(0) {}

DtlsIdentityStore::~DtlsIdentityStore() {}

void DtlsIdentityStore::Initialize() {
  GenerateIdentity();
}

void DtlsIdentityStore::RequestIdentity(DTLSIdentityRequestObserver* observer) {
  DCHECK(rtc::Thread::Current() == signaling_thread_);
  DCHECK(observer);

  // Must return the free identity async.
  if (free_identity_.get()) {
    IdentityResultMessageData* msg =
        new IdentityResultMessageData(free_identity_.release());
    signaling_thread_->Post(this, MSG_RETURN_FREE_IDENTITY, msg);
  }

  pending_observers_.push(observer);
  GenerateIdentity();
}

void DtlsIdentityStore::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_GENERATE_IDENTITY:
      GenerateIdentity_w();
      break;
    case MSG_GENERATE_IDENTITY_RESULT: {
      rtc::scoped_ptr<IdentityResultMessageData> pdata(
          static_cast<IdentityResultMessageData*>(msg->pdata));
      OnIdentityGenerated(pdata->data().Pass());
      break;
    }
    case MSG_RETURN_FREE_IDENTITY: {
      rtc::scoped_ptr<IdentityResultMessageData> pdata(
          static_cast<IdentityResultMessageData*>(msg->pdata));
      ReturnIdentity(pdata->data().Pass());
      break;
    }
  }
}

bool DtlsIdentityStore::HasFreeIdentityForTesting() const {
  return free_identity_.get();
}

void DtlsIdentityStore::GenerateIdentity() {
  pending_jobs_++;
  LOG(LS_VERBOSE) << "New DTLS identity generation is posted, "
                  << "pending_identities=" << pending_jobs_;
  worker_thread_->Post(this, MSG_GENERATE_IDENTITY, NULL);
}

void DtlsIdentityStore::OnIdentityGenerated(
    rtc::scoped_ptr<rtc::SSLIdentity> identity) {
  DCHECK(rtc::Thread::Current() == signaling_thread_);

  pending_jobs_--;
  LOG(LS_VERBOSE) << "A DTLS identity generation job returned, "
                  << "pending_identities=" << pending_jobs_;

  if (pending_observers_.empty()) {
    if (!free_identity_.get()) {
      free_identity_.reset(identity.release());
      LOG(LS_VERBOSE) << "A free DTLS identity is saved";
    }
    return;
  }
  ReturnIdentity(identity.Pass());
}

void DtlsIdentityStore::ReturnIdentity(
    rtc::scoped_ptr<rtc::SSLIdentity> identity) {
  DCHECK(!free_identity_.get());
  DCHECK(!pending_observers_.empty());

  rtc::scoped_refptr<DTLSIdentityRequestObserver> observer =
      pending_observers_.front();
  pending_observers_.pop();

  if (identity.get()) {
    observer->OnSuccessWithIdentityObj(identity.Pass());
  } else {
    // Pass an arbitrary error code.
    observer->OnFailure(0);
    LOG(LS_WARNING) << "Failed to generate SSL identity";
  }

  if (pending_observers_.empty() && pending_jobs_ == 0) {
    // Generate a free identity in the background.
    GenerateIdentity();
  }
}

void DtlsIdentityStore::GenerateIdentity_w() {
  DCHECK(rtc::Thread::Current() == worker_thread_);

  rtc::SSLIdentity* identity = rtc::SSLIdentity::Generate(kIdentityName);

  IdentityResultMessageData* msg = new IdentityResultMessageData(identity);
  signaling_thread_->Post(this, MSG_GENERATE_IDENTITY_RESULT, msg);
}

}  // namespace webrtc
