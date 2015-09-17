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

using webrtc::DtlsIdentityRequestObserver;

namespace webrtc {

// Passed to SSLIdentity::Generate, "WebRTC". Used for the certificates'
// subject and issuer name.
const char kIdentityName[] = "WebRTC";

namespace {

enum {
  MSG_DESTROY,
  MSG_GENERATE_IDENTITY,
  MSG_GENERATE_IDENTITY_RESULT
};

}  // namespace

// This class runs on the worker thread to generate the identity. It's necessary
// to separate this class from DtlsIdentityStore so that it can live on the
// worker thread after DtlsIdentityStore is destroyed.
class DtlsIdentityStoreImpl::WorkerTask : public sigslot::has_slots<>,
                                          public rtc::MessageHandler {
 public:
  WorkerTask(DtlsIdentityStoreImpl* store, rtc::KeyType key_type)
      : signaling_thread_(rtc::Thread::Current()),
        store_(store),
        key_type_(key_type) {
    store_->SignalDestroyed.connect(this, &WorkerTask::OnStoreDestroyed);
  }

  virtual ~WorkerTask() { RTC_DCHECK(signaling_thread_->IsCurrent()); }

 private:
  void GenerateIdentity_w() {
    LOG(LS_INFO) << "Generating identity, using keytype " << key_type_;
    rtc::scoped_ptr<rtc::SSLIdentity> identity(
        rtc::SSLIdentity::Generate(kIdentityName, key_type_));

    // Posting to |this| avoids touching |store_| on threads other than
    // |signaling_thread_| and thus avoids having to use locks.
    IdentityResultMessageData* msg = new IdentityResultMessageData(
        new IdentityResult(key_type_, identity.Pass()));
    signaling_thread_->Post(this, MSG_GENERATE_IDENTITY_RESULT, msg);
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
      case MSG_GENERATE_IDENTITY_RESULT:
        RTC_DCHECK(signaling_thread_->IsCurrent());
        {
          rtc::scoped_ptr<IdentityResultMessageData> pdata(
              static_cast<IdentityResultMessageData*>(msg->pdata));
          if (store_) {
            store_->OnIdentityGenerated(pdata->data()->key_type_,
                                        pdata->data()->identity_.Pass());
          }
        }
        break;
      case MSG_DESTROY:
        RTC_DCHECK(signaling_thread_->IsCurrent());
        delete msg->pdata;
        // |this| has now been deleted. Don't touch member variables.
        break;
      default:
        RTC_CHECK(false) << "Unexpected message type";
    }
  }

  void OnStoreDestroyed() {
    RTC_DCHECK(signaling_thread_->IsCurrent());
    store_ = nullptr;
  }

  rtc::Thread* const signaling_thread_;
  DtlsIdentityStoreImpl* store_;  // Only touched on |signaling_thread_|.
  const rtc::KeyType key_type_;
};

DtlsIdentityStoreImpl::DtlsIdentityStoreImpl(rtc::Thread* signaling_thread,
                                             rtc::Thread* worker_thread)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      request_info_() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  // Preemptively generate identities unless the worker thread and signaling
  // thread are the same (only do preemptive work in the background).
  if (worker_thread_ != signaling_thread_) {
    // Only necessary for RSA.
    GenerateIdentity(rtc::KT_RSA, nullptr);
  }
}

DtlsIdentityStoreImpl::~DtlsIdentityStoreImpl() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  SignalDestroyed();
}

void DtlsIdentityStoreImpl::RequestIdentity(
    rtc::KeyType key_type,
    const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>& observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(observer);

  GenerateIdentity(key_type, observer);
}

void DtlsIdentityStoreImpl::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  switch (msg->message_id) {
    case MSG_GENERATE_IDENTITY_RESULT: {
      rtc::scoped_ptr<IdentityResultMessageData> pdata(
          static_cast<IdentityResultMessageData*>(msg->pdata));
      OnIdentityGenerated(pdata->data()->key_type_,
                          pdata->data()->identity_.Pass());
      break;
    }
  }
}

bool DtlsIdentityStoreImpl::HasFreeIdentityForTesting(
    rtc::KeyType key_type) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return request_info_[key_type].free_identity_.get() != nullptr;
}

void DtlsIdentityStoreImpl::GenerateIdentity(
    rtc::KeyType key_type,
    const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>& observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  // Enqueue observer to be informed when generation of |key_type| is completed.
  if (observer.get()) {
    request_info_[key_type].request_observers_.push(observer);

    // Already have a free identity generated?
    if (request_info_[key_type].free_identity_.get()) {
      // Return identity async - post even though we are on |signaling_thread_|.
      LOG(LS_VERBOSE) << "Using a free DTLS identity.";
      ++request_info_[key_type].gen_in_progress_counts_;
      IdentityResultMessageData* msg = new IdentityResultMessageData(
          new IdentityResult(key_type,
                             request_info_[key_type].free_identity_.Pass()));
      signaling_thread_->Post(this, MSG_GENERATE_IDENTITY_RESULT, msg);
      return;
    }

    // Free identity in the process of being generated?
    if (request_info_[key_type].gen_in_progress_counts_ ==
            request_info_[key_type].request_observers_.size()) {
      // No need to do anything, the free identity will be returned to the
      // observer in a MSG_GENERATE_IDENTITY_RESULT.
      return;
    }
  }

  // Enqueue/Post a worker task to do the generation.
  ++request_info_[key_type].gen_in_progress_counts_;
  WorkerTask* task = new WorkerTask(this, key_type);  // Post 1 task/request.
  // The WorkerTask is owned by the message data to make sure it will not be
  // leaked even if the task does not get run.
  WorkerTaskMessageData* msg = new WorkerTaskMessageData(task);
  worker_thread_->Post(task, MSG_GENERATE_IDENTITY, msg);
}

void DtlsIdentityStoreImpl::OnIdentityGenerated(
    rtc::KeyType key_type, rtc::scoped_ptr<rtc::SSLIdentity> identity) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  RTC_DCHECK(request_info_[key_type].gen_in_progress_counts_);
  --request_info_[key_type].gen_in_progress_counts_;

  rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver> observer;
  if (!request_info_[key_type].request_observers_.empty()) {
    observer = request_info_[key_type].request_observers_.front();
    request_info_[key_type].request_observers_.pop();
  }

  if (observer.get() == nullptr) {
    // No observer - store result in |free_identities_|.
    RTC_DCHECK(!request_info_[key_type].free_identity_.get());
    request_info_[key_type].free_identity_.swap(identity);
    if (request_info_[key_type].free_identity_.get())
      LOG(LS_VERBOSE) << "A free DTLS identity was saved.";
    else
      LOG(LS_WARNING) << "Failed to generate DTLS identity (preemptively).";
  } else {
    // Return the result to the observer.
    if (identity.get()) {
      LOG(LS_VERBOSE) << "A DTLS identity is returned to an observer.";
      observer->OnSuccess(identity.Pass());
    } else {
      LOG(LS_WARNING) << "Failed to generate DTLS identity.";
      observer->OnFailure(0);
    }

    // Preemptively generate another identity of the same type?
    if (worker_thread_ != signaling_thread_ && // Only do in background thread.
        key_type == rtc::KT_RSA &&             // Only necessary for RSA.
        !request_info_[key_type].free_identity_.get() &&
        request_info_[key_type].request_observers_.size() <=
            request_info_[key_type].gen_in_progress_counts_) {
      GenerateIdentity(key_type, nullptr);
    }
  }
}

}  // namespace webrtc
