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

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {
class DTLSIdentityRequestObserver;
class SSLIdentity;
class Thread;

// This class implements an in-memory DTLS identity store, which generates the
// DTLS identity on the worker thread.
// APIs calls must be made on the signaling thread and the callbacks are also
// called on the signaling thread.
class DtlsIdentityStore : public rtc::MessageHandler {
 public:
  static const char kIdentityName[];

  DtlsIdentityStore(rtc::Thread* signaling_thread,
                    rtc::Thread* worker_thread);
  ~DtlsIdentityStore();

  // Initialize will start generating the free identity in the background.
  void Initialize();

  // The |observer| will be called when the requested identity is ready, or when
  // identity generation fails.
  void RequestIdentity(webrtc::DTLSIdentityRequestObserver* observer);

  // rtc::MessageHandler override;
  void OnMessage(rtc::Message* msg) override;

  // Returns true if there is a free identity, used for unit tests.
  bool HasFreeIdentityForTesting() const;

 private:
  void GenerateIdentity();
  void OnIdentityGenerated(rtc::scoped_ptr<rtc::SSLIdentity> identity);
  void ReturnIdentity(rtc::scoped_ptr<rtc::SSLIdentity> identity);

  void GenerateIdentity_w();

  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;

  // These members should be accessed on the signaling thread only.
  int pending_jobs_;
  rtc::scoped_ptr<rtc::SSLIdentity> free_identity_;
  typedef std::queue<rtc::scoped_refptr<webrtc::DTLSIdentityRequestObserver>>
      ObserverList;
  ObserverList pending_observers_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DTLSIDENTITYSTORE_H_
