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
  MSG_DESTROY,
  MSG_GENERATE_IDENTITY,
  MSG_GENERATE_IDENTITY_RESULT,
  MSG_RETURN_FREE_IDENTITY
};

typedef rtc::ScopedMessageData<rtc::SSLIdentity> IdentityResultMessageData;

}  // namespace

// This class runs on the worker thread to generate the identity. It's necessary
// to separate this class from DtlsIdentityStore so that it can live on the
// worker thread after DtlsIdentityStore is destroyed.
class DtlsIdentityStore::WorkerTask : public sigslot::has_slots<>,
                                      public rtc::MessageHandler {
 public:
  explicit WorkerTask(DtlsIdentityStore* store)
      : signaling_thread_(rtc::Thread::Current()), store_(store) {
    store_->SignalDestroyed.connect(this, &WorkerTask::OnStoreDestroyed);
  }

  virtual ~WorkerTask() { DCHECK(rtc::Thread::Current() == signaling_thread_); }

 private:
  void GenerateIdentity_w() {
    rtc::scoped_ptr<rtc::SSLIdentity> identity(
        rtc::SSLIdentity::Generate(DtlsIdentityStore::kIdentityName));

    {
      rtc::CritScope cs(&cs_);
      if (store_) {
        store_->PostGenerateIdentityResult_w(identity.Pass());
      }
    }
  }

  void OnMessage(rtc::Message* msg) override {
    switch (msg->message_id) {
      case MSG_GENERATE_IDENTITY:
        // This message always runs on the worker thread.
        GenerateIdentity_w();

        // Must delete |this|, owned by msg->pdata, on the signaling thread to
        // avoid races on disconnecting the signal.
        signaling_thread_->Post(this, MSG_DESTROY, msg->pdata);
        break;
      case MSG_DESTROY:
        DCHECK(rtc::Thread::Current() == signaling_thread_);
        delete msg->pdata;
        // |this| has now been deleted. Don't touch member variables.
        break;
      default:
        CHECK(false) << "Unexpected message type";
    }
  }

  void OnStoreDestroyed() {
    rtc::CritScope cs(&cs_);
    store_ = NULL;
  }

  rtc::Thread* const signaling_thread_;
  rtc::CriticalSection cs_;
  DtlsIdentityStore* store_;
};

// Arbitrary constant used as common name for the identity.
// Chosen to make the certificates more readable.
const char DtlsIdentityStore::kIdentityName[] = "WebRTC";

DtlsIdentityStore::DtlsIdentityStore(rtc::Thread* signaling_thread,
                                     rtc::Thread* worker_thread)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      pending_jobs_(0) {}

DtlsIdentityStore::~DtlsIdentityStore() {
  SignalDestroyed();
}

void DtlsIdentityStore::Initialize() {
  DCHECK(rtc::Thread::Current() == signaling_thread_);
  // Do not aggressively generate the free identity if the worker thread and the
  // signaling thread are the same.
  if (worker_thread_ != signaling_thread_) {
    GenerateIdentity();
  }
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
  DCHECK(rtc::Thread::Current() == signaling_thread_);
  switch (msg->message_id) {
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
  DCHECK(rtc::Thread::Current() == signaling_thread_);
  return free_identity_.get() != nullptr;
}

void DtlsIdentityStore::GenerateIdentity() {
  DCHECK(rtc::Thread::Current() == signaling_thread_);
  pending_jobs_++;
  LOG(LS_VERBOSE) << "New DTLS identity generation is posted, "
                  << "pending_identities=" << pending_jobs_;

  WorkerTask* task = new WorkerTask(this);
  // The WorkerTask is owned by the message data to make sure it will not be
  // leaked even if the task does not get run.
  IdentityTaskMessageData* msg = new IdentityTaskMessageData(task);
  worker_thread_->Post(task, MSG_GENERATE_IDENTITY, msg);
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
  DCHECK(rtc::Thread::Current() == signaling_thread_);
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

  // Do not aggressively generate the free identity if the worker thread and the
  // signaling thread are the same.
  if (worker_thread_ != signaling_thread_ &&
      pending_observers_.empty() &&
      pending_jobs_ == 0) {
    // Generate a free identity in the background.
    GenerateIdentity();
  }
}

void DtlsIdentityStore::PostGenerateIdentityResult_w(
    rtc::scoped_ptr<rtc::SSLIdentity> identity) {
  DCHECK(rtc::Thread::Current() == worker_thread_);

  IdentityResultMessageData* msg =
      new IdentityResultMessageData(identity.release());
  signaling_thread_->Post(this, MSG_GENERATE_IDENTITY_RESULT, msg);
}
}  // namespace webrtc
