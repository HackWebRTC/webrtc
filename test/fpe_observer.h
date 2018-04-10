/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FPE_OBSERVER_H_
#define TEST_FPE_OBSERVER_H_

#include <cfenv>
#include "test/gtest.h"

namespace webrtc {
namespace test {

// Class that let a unit test fail if floating point exceptions are signaled.
// Usage:
// {
//   FloatingPointExceptionObserver fpe_observer;
//   ...
// }
class FloatingPointExceptionObserver {
 public:
  FloatingPointExceptionObserver(int mask = FE_DIVBYZERO | FE_INVALID |
                                            FE_OVERFLOW | FE_UNDERFLOW)
      : mask_(mask) {
#ifdef NDEBUG
    EXPECT_LE(0, mask_);  // Avoid compile time errors in release mode.
#else
    EXPECT_EQ(0, std::feclearexcept(mask_));
#endif
  }
  ~FloatingPointExceptionObserver() {
#ifndef NDEBUG
    const int occurred = std::fetestexcept(mask_);
    EXPECT_FALSE(occurred & FE_INVALID)
        << "Domain error occurred in a floating-point operation.";
    EXPECT_FALSE(occurred & FE_DIVBYZERO) << "Division by zero.";
    EXPECT_FALSE(occurred & FE_OVERFLOW)
        << "The result of a floating-point operation was too large.";
    EXPECT_FALSE(occurred & FE_UNDERFLOW)
        << "The result of a floating-point operation was subnormal with a loss "
        << "of precision.";
    EXPECT_FALSE(occurred & FE_INEXACT)
        << "Inexact result: rounding during a floating-point operation.";
#endif
  }

 private:
  const int mask_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FPE_OBSERVER_H_
