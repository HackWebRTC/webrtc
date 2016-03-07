/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_DTLSIDENTITYSTORE_H_
#define WEBRTC_API_DTLSIDENTITYSTORE_H_

#include <queue>
#include <string>
#include <utility>

#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/optional.h"
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
  // TODO(torbjorng,hbos): There are currently two versions of RequestIdentity,
  // with default implementation to call the other version of itself (so that a
  // call can be made regardless of which version has been overridden). The 1st
  // version exists because it is currently implemented in chromium. The 2nd
  // version will become the one and only RequestIdentity as soon as chromium
  // implements the correct version. crbug.com/544902, webrtc:5092.
  virtual void RequestIdentity(
      rtc::KeyParams key_params,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer) {
    // Add default ("null") expiration.
    RequestIdentity(key_params, rtc::Optional<uint64_t>(), observer);
  }
  virtual void RequestIdentity(
      const rtc::KeyParams& key_params,
      const rtc::Optional<uint64_t>& expires_ms,
      const rtc::scoped_refptr<DtlsIdentityRequestObserver>& observer) {
    // Drop |expires|.
    RequestIdentity(key_params, observer);
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
      const rtc::KeyParams& key_params,
      const rtc::Optional<uint64_t>& expires_ms,
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

#endif  // WEBRTC_API_DTLSIDENTITYSTORE_H_
