/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/rtc_base/nethelpers.h"

#include <memory>

#if defined(WEBRTC_WIN)
#include <ws2spi.h>
#include <ws2tcpip.h>
#include "webrtc/rtc_base/win32.h"
#endif
#if defined(WEBRTC_POSIX) && !defined(__native_client__)
#if defined(WEBRTC_ANDROID)
#include "webrtc/rtc_base/ifaddrs-android.h"
#else
#include <ifaddrs.h>
#endif
#endif  // defined(WEBRTC_POSIX) && !defined(__native_client__)

#include "webrtc/rtc_base/bind.h"
#include "webrtc/rtc_base/byteorder.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/rtc_base/task_queue.h"
#include "webrtc/rtc_base/thread.h"

namespace rtc {

namespace {
int ResolveHostname(const std::string& hostname, int family,
                    std::vector<IPAddress>* addresses) {
#ifdef __native_client__
  RTC_NOTREACHED();
  LOG(LS_WARNING) << "ResolveHostname() is not implemented for NaCl";
  return -1;
#else  // __native_client__
  if (!addresses) {
    return -1;
  }
  addresses->clear();
  struct addrinfo* result = nullptr;
  struct addrinfo hints = {0};
  hints.ai_family = family;
  // |family| here will almost always be AF_UNSPEC, because |family| comes from
  // AsyncResolver::addr_.family(), which comes from a SocketAddress constructed
  // with a hostname. When a SocketAddress is constructed with a hostname, its
  // family is AF_UNSPEC. However, if someday in the future we construct
  // a SocketAddress with both a hostname and a family other than AF_UNSPEC,
  // then it would be possible to get a specific family value here.

  // The behavior of AF_UNSPEC is roughly "get both ipv4 and ipv6", as
  // documented by the various operating systems:
  // Linux: http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
  // Windows: https://msdn.microsoft.com/en-us/library/windows/desktop/
  // ms738520(v=vs.85).aspx
  // Mac: https://developer.apple.com/legacy/library/documentation/Darwin/
  // Reference/ManPages/man3/getaddrinfo.3.html
  // Android (source code, not documentation):
  // https://android.googlesource.com/platform/bionic/+/
  // 7e0bfb511e85834d7c6cb9631206b62f82701d60/libc/netbsd/net/getaddrinfo.c#1657
  hints.ai_flags = AI_ADDRCONFIG;
  int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
  if (ret != 0) {
    return ret;
  }
  struct addrinfo* cursor = result;
  for (; cursor; cursor = cursor->ai_next) {
    if (family == AF_UNSPEC || cursor->ai_family == family) {
      IPAddress ip;
      if (IPFromAddrInfo(cursor, &ip)) {
        addresses->push_back(ip);
      }
    }
  }
  freeaddrinfo(result);
  return 0;
#endif  // !__native_client__
}
}  // namespace

// AsyncResolver
AsyncResolver::AsyncResolver() : construction_thread_(Thread::Current()) {
  RTC_DCHECK(construction_thread_);
}

AsyncResolver::~AsyncResolver() {
  RTC_DCHECK(construction_thread_->IsCurrent());
  if (state_)
    // It's possible that we have a posted message waiting on the MessageQueue
    // refering to this object. Indirection via the ref-counted state_ object
    // ensure it doesn't access us after deletion.

    // TODO(nisse): An alternative approach to solve this problem would be to
    // extend MessageQueue::Clear in some way to let us selectively cancel posts
    // directed to this object. Then we wouldn't need any ref count, but its a
    // larger change to the MessageQueue.
    state_->resolver = nullptr;
}

void AsyncResolver::Start(const SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(construction_thread_);
  RTC_DCHECK(!resolver_queue_);
  RTC_DCHECK(!state_);
  // TODO(nisse): Support injection of task queue at construction?
  resolver_queue_ = rtc::MakeUnique<TaskQueue>("AsyncResolverQueue");
  addr_ = addr;
  state_ = new RefCountedObject<Trampoline>(this);

  // These member variables need to be copied to local variables to make it
  // possible to capture them, even for capture-by-copy.
  scoped_refptr<Trampoline> state = state_;
  rtc::Thread* construction_thread = construction_thread_;
  resolver_queue_->PostTask([state, addr, construction_thread]() {
    std::vector<IPAddress> addresses;
    int error =
        ResolveHostname(addr.hostname().c_str(), addr.family(), &addresses);
    // Ensure SignalDone is called on the main thread.
    // TODO(nisse): Should use move of the address list, but not easy until
    // C++17. Since this code isn't performance critical, copy should be fine
    // for now.
    construction_thread->Post(RTC_FROM_HERE, [state, error, addresses]() {
      if (!state->resolver)
        return;
      state->resolver->ResolveDone(error, std::move(addresses));
    });
  });
}

bool AsyncResolver::GetResolvedAddress(int family, SocketAddress* addr) const {
  if (error_ != 0 || addresses_.empty())
    return false;

  *addr = addr_;
  for (size_t i = 0; i < addresses_.size(); ++i) {
    if (family == addresses_[i].family()) {
      addr->SetResolvedIP(addresses_[i]);
      return true;
    }
  }
  return false;
}

int AsyncResolver::GetError() const {
  return error_;
}

void AsyncResolver::Destroy(bool wait) {
  RTC_DCHECK_RUN_ON(construction_thread_);
  RTC_DCHECK(!state_ || state_->resolver);
  // If we don't wait here, we will nevertheless wait in the destructor.
  if (wait || !state_) {
    // Destroy task queue, blocks on any currently running task. If we have a
    // pending task, it will post a call to attempt to call ResolveDone before
    // finishing, which we will never handle.
    delete this;
  } else {
    destroyed_ = true;
  }
}

void AsyncResolver::ResolveDone(int error, std::vector<IPAddress> addresses) {
  RTC_DCHECK_RUN_ON(construction_thread_);
  error_ = error;
  addresses_ = std::move(addresses);
  if (destroyed_) {
    delete this;
    return;
  } else {
    // Beware that SignalDone may call Destroy.

    // TODO(nisse): Currently allows only Destroy(false) in this case,
    // and that's what all webrtc code is using. With Destroy(true),
    // this object would be destructed immediately, and the access
    // both to |destroyed_| below as well as the sigslot machinery
    // involved in SignalDone implies invalid use-after-free.
    SignalDone(this);
    if (destroyed_) {
      delete this;
      return;
    }
  }
  state_ = nullptr;
}

const char* inet_ntop(int af, const void *src, char* dst, socklen_t size) {
#if defined(WEBRTC_WIN)
  return win32_inet_ntop(af, src, dst, size);
#else
  return ::inet_ntop(af, src, dst, size);
#endif
}

int inet_pton(int af, const char* src, void *dst) {
#if defined(WEBRTC_WIN)
  return win32_inet_pton(af, src, dst);
#else
  return ::inet_pton(af, src, dst);
#endif
}

bool HasIPv4Enabled() {
#if defined(WEBRTC_POSIX) && !defined(__native_client__)
  bool has_ipv4 = false;
  struct ifaddrs* ifa;
  if (getifaddrs(&ifa) < 0) {
    return false;
  }
  for (struct ifaddrs* cur = ifa; cur != nullptr; cur = cur->ifa_next) {
    if (cur->ifa_addr->sa_family == AF_INET) {
      has_ipv4 = true;
      break;
    }
  }
  freeifaddrs(ifa);
  return has_ipv4;
#else
  return true;
#endif
}

bool HasIPv6Enabled() {
#if defined(WEBRTC_WIN)
  if (IsWindowsVistaOrLater()) {
    return true;
  }
  if (!IsWindowsXpOrLater()) {
    return false;
  }
  DWORD protbuff_size = 4096;
  std::unique_ptr<char[]> protocols;
  LPWSAPROTOCOL_INFOW protocol_infos = nullptr;
  int requested_protocols[2] = {AF_INET6, 0};

  int err = 0;
  int ret = 0;
  // Check for protocols in a do-while loop until we provide a buffer large
  // enough. (WSCEnumProtocols sets protbuff_size to its desired value).
  // It is extremely unlikely that this will loop more than once.
  do {
    protocols.reset(new char[protbuff_size]);
    protocol_infos = reinterpret_cast<LPWSAPROTOCOL_INFOW>(protocols.get());
    ret = WSCEnumProtocols(requested_protocols, protocol_infos,
                           &protbuff_size, &err);
  } while (ret == SOCKET_ERROR && err == WSAENOBUFS);

  if (ret == SOCKET_ERROR) {
    return false;
  }

  // Even if ret is positive, check specifically for IPv6.
  // Non-IPv6 enabled WinXP will still return a RAW protocol.
  for (int i = 0; i < ret; ++i) {
    if (protocol_infos[i].iAddressFamily == AF_INET6) {
      return true;
    }
  }
  return false;
#elif defined(WEBRTC_POSIX) && !defined(__native_client__)
  bool has_ipv6 = false;
  struct ifaddrs* ifa;
  if (getifaddrs(&ifa) < 0) {
    return false;
  }
  for (struct ifaddrs* cur = ifa; cur != nullptr; cur = cur->ifa_next) {
    if (cur->ifa_addr->sa_family == AF_INET6) {
      has_ipv6 = true;
      break;
    }
  }
  freeifaddrs(ifa);
  return has_ipv6;
#else
  return true;
#endif
}
}  // namespace rtc
