/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FUNCTION_VIEW_H_
#define WEBRTC_BASE_FUNCTION_VIEW_H_

#include <type_traits>
#include <utility>

// Just like std::function, FunctionView will wrap any callable and hide its
// actual type, exposing only its signature. But unlike std::function,
// FunctionView doesn't own its callable---it just points to it. Thus, it's a
// good choice mainly as a function argument when the callable argument will
// not be called again once the function has returned.
//
// TODO(kwiberg): FunctionView doesn't work with function pointers, just with
// lambdas. It's trivial to work around this by wrapping the function pointer
// in a stateless lambda, but it's tedious so it'd be nice to not have to do
// it.

namespace rtc {

template <typename T>
class FunctionView;  // Undefined.

template <typename RetT, typename... ArgT>
class FunctionView<RetT(ArgT...)> final {
 public:
  // This constructor is implicit, so that callers won't have to convert
  // lambdas and other callables to FunctionView<Blah(Blah, Blah)> explicitly.
  // This is safe because FunctionView is only a reference to the real
  // callable.
  //
  // We jump through some template metaprogramming hoops to ensure that this
  // constructor does *not* accept FunctionView arguments. That way, copy
  // construction, assignment, swap etc. will all do the obvious thing (because
  // they use the implicitly-declared copy constructor and copy assignment),
  // and we will never get a FunctionView object that points to another
  // FunctionView.
  template <typename F,
            typename std::enable_if<!std::is_same<
                FunctionView,
                typename std::remove_cv<typename std::remove_reference<
                    F>::type>::type>::value>::type* = nullptr>
  FunctionView(F&& f)
      : f_(&f), call_(Call<typename std::remove_reference<F>::type>) {}

  RetT operator()(ArgT... args) const {
    return call_(f_, std::forward<ArgT>(args)...);
  }

 private:
  template <typename F>
  static RetT Call(void* f, ArgT... args) {
    return (*static_cast<F*>(f))(std::forward<ArgT>(args)...);
  }
  void* f_;
  RetT (*call_)(void* f, ArgT... args);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FUNCTION_VIEW_H_
