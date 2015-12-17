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

#ifndef TALK_APP_WEBRTC_DTLSIDENTITYSTORE_H_
#define TALK_APP_WEBRTC_DTLSIDENTITYSTORE_H_

#include <queue>
#include <string>
#include <utility>

#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/thread.h"

namespace webrtc {

// Passed to SSLIdentity::Generate.
extern const char kIdentityName[];

class SSLIdentity;
class Thread;

// Used to receive callbacks of DTLS identity requests.
class DtlsIdentityRequestObserver : public rtc::RefCountInterface {
 public:
  virtual void OnFailure(int error) = 0;
  // TODO(hbos): Unify the OnSuccess method once Chrome code is updated.
  virtual void OnSuccess(const std::string& der_cert,
                         const std::string& der_private_key) = 0;
  // |identity| is a scoped_ptr because rtc::SSLIdentity is not copyable and the
  // client has to get the ownership of the object to make use of it.
  virtual void OnSuccess(rtc::scoped_ptr<rtc::SSLIdentity> identity) = 0;

 protected:
  virtual ~DtlsIdentityRequestObserver() {}
};

// This interface defines an in-memory DTLS identity store, which generates DTLS
// identities.
// APIs calls must be made on the signaling thread and the callbacks are also
// called on the signaling thread.
class DtlsIdentityStoreInterface {
 public:
  virtual ~DtlsIdentityStoreInterface() { }

  // The |observer| will be called when the requested identity is ready, or when
  // identity generation fails.
  // TODO(torbjorng,hbos): The following RequestIdentity is about to be removed,
  // see below todo.
  virtual void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer) {
    // Add default parameterization.
    RequestIdentity(rtc::KeyParams(key_type), observer);
  }
  // TODO(torbjorng,hbos): Parameterized key types! The following
  // RequestIdentity should replace the old one that takes rtc::KeyType. When
  // the new one is implemented by Chromium and WebRTC the old one should be
  // removed. crbug.com/544902, webrtc:5092.
  virtual void RequestIdentity(
      rtc::KeyParams key_params,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer) {
    // Drop parameterization.
    RequestIdentity(key_params.type(), observer);
  }
};

// The WebRTC default implementation of DtlsIdentityStoreInterface.
// Identity generation is performed on the worker thread.
class DtlsIdentityStoreImpl : public DtlsIdentityStoreInterface,
                              public rtc::MessageHandler {
 public:
  // This will start to preemptively generating an RSA identity in the
  // background if the worker thread is not the same as the signaling thread.
  DtlsIdentityStoreImpl(rtc::Thread* signaling_thread,
                        rtc::Thread* worker_thread);
  ~DtlsIdentityStoreImpl() override;

  // DtlsIdentityStoreInterface override;
  void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer) override;

  // rtc::MessageHandler override;
  void OnMessage(rtc::Message* msg) override;

  // Returns true if there is a free RSA identity, used for unit tests.
  bool HasFreeIdentityForTesting(rtc::KeyType key_type) const;

 private:
  void GenerateIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer);
  void OnIdentityGenerated(rtc::KeyType key_type,
                           rtc::scoped_ptr<rtc::SSLIdentity> identity);

  class WorkerTask;
  typedef rtc::ScopedMessageData<DtlsIdentityStoreImpl::WorkerTask>
      WorkerTaskMessageData;

  // A key type-identity pair.
  struct IdentityResult {
    IdentityResult(rtc::KeyType key_type,
                   rtc::scoped_ptr<rtc::SSLIdentity> identity)
        : key_type_(key_type), identity_(std::move(identity)) {}

    rtc::KeyType key_type_;
    rtc::scoped_ptr<rtc::SSLIdentity> identity_;
  };

  typedef rtc::ScopedMessageData<IdentityResult> IdentityResultMessageData;

  sigslot::signal0<> SignalDestroyed;

  rtc::Thread* const signaling_thread_;
  // TODO(hbos): RSA generation is slow and would be VERY slow if we switch over
  // to 2048, DtlsIdentityStore should use a new thread and not the "general
  // purpose" worker thread.
  rtc::Thread* const worker_thread_;

  struct RequestInfo {
    RequestInfo()
        : request_observers_(), gen_in_progress_counts_(0), free_identity_() {}

    std::queue<rtc::scoped_refptr<DtlsIdentityRequestObserver>>
        request_observers_;
    size_t gen_in_progress_counts_;
    rtc::scoped_ptr<rtc::SSLIdentity> free_identity_;
  };

  // One RequestInfo per KeyType. Only touch on the |signaling_thread_|.
  RequestInfo request_info_[rtc::KT_LAST];
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DTLSIDENTITYSTORE_H_
