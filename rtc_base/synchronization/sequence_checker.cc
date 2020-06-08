/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_checker.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>

#if defined(WEBRTC_MAC)
#include <dispatch/dispatch.h>
#endif

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
// On Mac, returns the label of the current dispatch queue; elsewhere, return
// null.
const void* GetSystemQueueRef() {
#if defined(WEBRTC_MAC)
  return dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL);
#else
  return nullptr;
#endif
}

template <class T,
          typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr>
uintptr_t CastToUintPtr(T t) {
  return reinterpret_cast<uintptr_t>(t);
}

template <class T,
          typename std::enable_if<!std::is_pointer<T>::value>::type* = nullptr>
uintptr_t CastToUintPtr(T t) {
  return static_cast<uintptr_t>(t);
}

}  // namespace

std::string ExpectationToString(const webrtc::SequenceChecker* checker) {
#if RTC_DCHECK_IS_ON
  return checker->ExpectationToString();
#endif
  return std::string();
}

SequenceCheckerImpl::SequenceCheckerImpl()
    : attached_(true),
      valid_thread_(rtc::CurrentThreadRef()),
      valid_queue_(TaskQueueBase::Current()),
      valid_system_queue_(GetSystemQueueRef()) {}

SequenceCheckerImpl::~SequenceCheckerImpl() = default;

bool SequenceCheckerImpl::IsCurrent() const {
  const TaskQueueBase* const current_queue = TaskQueueBase::Current();
  const rtc::PlatformThreadRef current_thread = rtc::CurrentThreadRef();
  const void* const current_system_queue = GetSystemQueueRef();
  rtc::CritScope scoped_lock(&lock_);
  if (!attached_) {  // Previously detached.
    attached_ = true;
    valid_thread_ = current_thread;
    valid_queue_ = current_queue;
    valid_system_queue_ = current_system_queue;
    return true;
  }
  if (valid_queue_ || current_queue) {
    return valid_queue_ == current_queue;
  }
  if (valid_system_queue_ && valid_system_queue_ == current_system_queue) {
    return true;
  }
  return rtc::IsThreadRefEqual(valid_thread_, current_thread);
}

void SequenceCheckerImpl::Detach() {
  rtc::CritScope scoped_lock(&lock_);
  attached_ = false;
  // We don't need to touch the other members here, they will be
  // reset on the next call to IsCurrent().
}

#if RTC_DCHECK_IS_ON
std::string SequenceCheckerImpl::ExpectationToString() const {
  const TaskQueueBase* const current_queue = TaskQueueBase::Current();
  const rtc::PlatformThreadRef current_thread = rtc::CurrentThreadRef();
  const void* const current_system_queue = GetSystemQueueRef();
  rtc::CritScope scoped_lock(&lock_);
  if (!attached_)
    return "Checker currently not attached.";

  // NOTE: The format of thie string built here is meant to compliment the one
  // we have inside of FatalLog() (checks.cc).
  //
  // Example:
  //
  // Expectations vs Actual:
  // # Exp: TQ: 0000000000000000 SysQ: 00007fff69541330 Thread: 0000000113aafdc0
  // # Act: TQ: 00007fcde7a22210 SysQ: 00007fcde78553c0 Thread: 0000700005ddc000
  // TaskQueue doesn't match

  rtc::StringBuilder message;
  message.AppendFormat(
      "Expectations vs Actual:\n# Exp: "
      "TQ: %016" PRIxPTR " SysQ: %016" PRIxPTR " Thread: %016" PRIxPTR
      "\n# Act: "
      "TQ: %016" PRIxPTR " SysQ: %016" PRIxPTR " Thread: %016" PRIxPTR "\n",
      CastToUintPtr(valid_queue_), CastToUintPtr(valid_system_queue_),
      CastToUintPtr(valid_thread_), CastToUintPtr(current_queue),
      CastToUintPtr(current_system_queue), CastToUintPtr(current_thread));

  if ((valid_queue_ || current_queue) && valid_queue_ != current_queue) {
    message << "TaskQueue doesn't match\n";
  } else if (valid_system_queue_ &&
             valid_system_queue_ != current_system_queue) {
    message << "System queue doesn't match\n";
  } else if (!rtc::IsThreadRefEqual(valid_thread_, current_thread)) {
    message << "Threads don't match\n";
  }

  return message.Release();
}
#endif  // RTC_DCHECK_IS_ON

}  // namespace webrtc
