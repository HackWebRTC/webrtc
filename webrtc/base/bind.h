/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Bind() is an overloaded function that converts method calls into function
// objects (aka functors). The method object is captured as a scoped_refptr<> if
// possible, and as a raw pointer otherwise. Any arguments to the method are
// captured by value. The return value of Bind is a stateful, nullary function
// object. Care should be taken about the lifetime of objects captured by
// Bind(); the returned functor knows nothing about the lifetime of a non
// ref-counted method object or any arguments passed by pointer, and calling the
// functor with a destroyed object will surely do bad things.
//
// To prevent the method object from being captured as a scoped_refptr<>, you
// can use Unretained. But this should only be done when absolutely necessary,
// and when the caller knows the extra reference isn't needed.
//
// Example usage:
//   struct Foo {
//     int Test1() { return 42; }
//     int Test2() const { return 52; }
//     int Test3(int x) { return x*x; }
//     float Test4(int x, float y) { return x + y; }
//   };
//
//   int main() {
//     Foo foo;
//     cout << rtc::Bind(&Foo::Test1, &foo)() << endl;
//     cout << rtc::Bind(&Foo::Test2, &foo)() << endl;
//     cout << rtc::Bind(&Foo::Test3, &foo, 3)() << endl;
//     cout << rtc::Bind(&Foo::Test4, &foo, 7, 8.5f)() << endl;
//   }
//
// Example usage of ref counted objects:
//   struct Bar {
//     int AddRef();
//     int Release();
//
//     void Test() {}
//     void BindThis() {
//       // The functor passed to AsyncInvoke() will keep this object alive.
//       invoker.AsyncInvoke(RTC_FROM_HERE,rtc::Bind(&Bar::Test, this));
//     }
//   };
//
//   int main() {
//     rtc::scoped_refptr<Bar> bar = new rtc::RefCountedObject<Bar>();
//     auto functor = rtc::Bind(&Bar::Test, bar);
//     bar = nullptr;
//     // The functor stores an internal scoped_refptr<Bar>, so this is safe.
//     functor();
//   }
//

#ifndef WEBRTC_BASE_BIND_H_
#define WEBRTC_BASE_BIND_H_


// This header is deprecated and is just left here temporarily during
// refactoring. See https://bugs.webrtc.org/7634 for more details.
#include "webrtc/rtc_base/bind.h"

#endif  // WEBRTC_BASE_BIND_H_
