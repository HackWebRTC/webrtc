/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_

#include <CoreFoundation/CoreFoundation.h>
namespace rtc {

template <typename T>
class ScopedCFTypeRef {
 public:
  // RETAIN: ScopedCFTypeRef should retain the object when it takes
  // ownership.
  // ASSUME: Assume the object already has already been retained.
  // ScopedCFTypeRef takes over ownership.
  enum class RetainPolicy { RETAIN, ASSUME };

  ScopedCFTypeRef() : ptr_(nullptr) {}
  explicit ScopedCFTypeRef(T ptr) : ptr_(ptr) {}
  ScopedCFTypeRef(T ptr, RetainPolicy policy) : ScopedCFTypeRef(ptr) {
    if (ptr_ && policy == RetainPolicy::RETAIN)
      CFRetain(ptr_);
  }

  ~ScopedCFTypeRef() {
    if (ptr_) {
      CFRelease(ptr_);
    }
  }

  T get() const { return ptr_; }
  T operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_; }

  bool operator!() const { return !ptr_; }

  ScopedCFTypeRef& operator=(const T& rhs) {
    if (ptr_)
      CFRelease(ptr_);
    ptr_ = rhs;
    return *this;
  }

  ScopedCFTypeRef& operator=(const ScopedCFTypeRef<T>& rhs) {
    reset(rhs.get(), RetainPolicy::RETAIN);
    return *this;
  }

  // This is intended to take ownership of objects that are
  // created by pass-by-pointer initializers.
  T* InitializeInto() {
    RTC_DCHECK(!ptr_);
    return &ptr_;
  }

  void reset(T ptr, RetainPolicy policy = RetainPolicy::ASSUME) {
    if (ptr && policy == RetainPolicy::RETAIN)
      CFRetain(ptr);
    if (ptr_)
      CFRelease(ptr_);
    ptr_ = ptr;
  }

  T release() {
    T temp = ptr_;
    ptr_ = nullptr;
    return temp;
  }

 private:
  T ptr_;
};

template <typename T>
static ScopedCFTypeRef<T> AdoptCF(T cftype) {
  return ScopedCFTypeRef<T>(cftype, RetainPolicy::RETAIN);
}

template <typename T>
static ScopedCFTypeRef<T> ScopedCF(T cftype) {
  return ScopedCFTypeRef<T>(cftype);
}

}  // namespace rtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_
