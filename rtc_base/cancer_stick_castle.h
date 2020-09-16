/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCER_STICK_CASTLE_H_
#define RTC_BASE_CANCER_STICK_CASTLE_H_

#include <utility>
#include <vector>

#include "api/function_view.h"
#include "rtc_base/function.h"
#include "rtc_base/system/assume.h"

namespace webrtc {
namespace cancer_stick_castle_impl {

class CancerStickCastleReceivers {
 public:
  CancerStickCastleReceivers();
  ~CancerStickCastleReceivers();
  void AddReceiver(UntypedFunction&& f) {
    AddReceiverImpl(&f);
    // Assume that f was moved from and is now trivially destructible.
    // This helps the compiler optimize away the destructor call.
    RTC_ASSUME(f.IsTriviallyDestructible());
  }
  void Foreach(rtc::FunctionView<void(UntypedFunction&)> fv);

 private:
  void AddReceiverImpl(UntypedFunction* f);
  std::vector<UntypedFunction> receivers_;
};

}  // namespace cancer_stick_castle_impl

// A collection of receivers (callable objects) that can be called all at once.
// Optimized for minimal binary size.
//
// TODO(kwiberg): Add support for removing receivers, if necessary. AddReceiver
// would have to return some sort of ID that the caller could save and then pass
// to RemoveReceiver. Alternatively, the callable objects could return one value
// if they wish to stay in the CSC and another value if they wish to be removed.
// It depends on what's convenient for the callers...
template <typename... ArgT>
class CancerStickCastle {
 public:
  template <typename F>
  void AddReceiver(F&& f) {
    receivers_.AddReceiver(
        UntypedFunction::Create<void(ArgT...)>(std::forward<F>(f)));
  }
  void Send(ArgT&&... args) {
    receivers_.Foreach([&](UntypedFunction& f) {
      f.Call<void(ArgT...)>(std::forward<ArgT>(args)...);
    });
  }

 private:
  cancer_stick_castle_impl::CancerStickCastleReceivers receivers_;
};

}  // namespace webrtc

#endif  // RTC_BASE_CANCER_STICK_CASTLE_H_
