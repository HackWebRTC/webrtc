/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_RTC_BASE_NETHELPERS_H_
#define WEBRTC_RTC_BASE_NETHELPERS_H_

#if defined(WEBRTC_POSIX)
#include <netdb.h>
#include <stddef.h>
#elif WEBRTC_WIN
#include <winsock2.h>  // NOLINT
#endif

#include <list>
#include <memory>

#include "webrtc/base/asyncresolverinterface.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread_checker.h"

namespace rtc {

class Thread;
class TaskQueue;

// AsyncResolver will perform async DNS resolution, signaling the result on the
// SignalDone from AsyncResolverInterface when the operation completes.
// SignalDone is fired on the same thread on which the AsyncResolver was
// constructed.
class AsyncResolver : public AsyncResolverInterface {
 public:
  AsyncResolver();
  ~AsyncResolver() override;

  void Start(const SocketAddress& addr) override;
  bool GetResolvedAddress(int family, SocketAddress* addr) const override;
  int GetError() const override;
  void Destroy(bool wait) override;

  const std::vector<IPAddress>& addresses() const { return addresses_; }

 private:
  void ResolveDone(int error, std::vector<IPAddress> addresses);

  class Trampoline : public RefCountInterface {
   public:
    Trampoline(AsyncResolver* resolver) : resolver(resolver) {}
    // Points back to the resolver, as long as it is alive. Cleared
    // by the AsyncResolver destructor.
    AsyncResolver* resolver;
  };

  // |state_| is non-null while resolution is pending, i.e., set
  // non-null by Start() and cleared by ResolveDone(). The destructor
  // clears state_->resolver (assuming |state_| is non-null), to
  // indicate that the resolver can no longer be accessed.
  scoped_refptr<Trampoline> state_ ACCESS_ON(construction_thread_);

  Thread* const construction_thread_;
  // Set to true when Destroy() can't delete the object immediately.
  // Indicate that the ResolveDone method is now responsible for
  // deletion. method should delete the object.
  bool destroyed_ = false;
  // Queue used only for a single task.
  std::unique_ptr<TaskQueue> resolver_queue_;
  SocketAddress addr_;
  std::vector<IPAddress> addresses_;
  int error_ = -1;
};

// rtc namespaced wrappers for inet_ntop and inet_pton so we can avoid
// the windows-native versions of these.
const char* inet_ntop(int af, const void *src, char* dst, socklen_t size);
int inet_pton(int af, const char* src, void *dst);

bool HasIPv4Enabled();
bool HasIPv6Enabled();
}  // namespace rtc

#endif  // WEBRTC_RTC_BASE_NETHELPERS_H_
