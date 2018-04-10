/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>

#include "rtc_base/logging.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

std::map<int, std::string> GetExceptionCodes() {
  static const std::map<int, std::string> codes = {
      {FE_INVALID, "FE_INVALID"},
// TODO(bugs.webrtc.org/8948): Some floating point exceptions are not signaled
// on Android.
#ifndef WEBRTC_ANDROID
      {FE_DIVBYZERO, "FE_DIVBYZERO"}, {FE_OVERFLOW, "FE_OVERFLOW"},
      {FE_UNDERFLOW, "FE_UNDERFLOW"},
#endif
      {FE_INEXACT, "FE_INEXACT"},
  };
  return codes;
}

// Define helper functions as a trick to trigger floating point exceptions at
// run-time.
float MinusOne() {
  return -std::cos(0.f);
}

float PlusOne() {
  return std::cos(0.f);
}

float PlusTwo() {
  return 2.f * std::cos(0.f);
}

// Triggers one or more exception according to the |trigger| mask while
// observing the floating point exceptions defined in the |observe| mask.
void TriggerObserveFloatingPointExceptions(int trigger, int observe) {
  FloatingPointExceptionObserver fpe_observer(observe);
  float tmp = 0.f;
  if (trigger & FE_INVALID)
    tmp = std::sqrt(MinusOne());
  if (trigger & FE_DIVBYZERO)
    tmp = 1.f / (MinusOne() + PlusOne());
  if (trigger & FE_OVERFLOW)
    tmp = std::numeric_limits<float>::max() * PlusTwo();
  if (trigger & FE_UNDERFLOW) {
    // TODO(bugs.webrtc.org/8948): Check why FE_UNDERFLOW is not triggered with
    // <float>.
    tmp = std::numeric_limits<double>::min() / PlusTwo();
  }
  if (trigger & FE_INEXACT) {
    tmp = std::sqrt(2.0);
  }
}

}  // namespace

TEST(FloatingPointExceptionObserverTest, CheckTestConstants) {
  // Check that the constants used in the test suite behave as expected.
  ASSERT_EQ(0.f, MinusOne() + PlusOne());
#ifndef WEBRTC_ANDROID
  // Check that all the floating point exceptions are exercised.
  int all_flags = 0;
  for (const auto v : GetExceptionCodes()) {
    RTC_LOG(LS_INFO) << v.second << " = " << v.first;
    all_flags |= v.first;
  }
#ifdef WEBRTC_MAC
#ifndef FE_UNNORMAL
#define FE_UNNORMAL 2
#endif
  all_flags |= FE_UNNORMAL;  // Non standard OS specific flag.
#endif
  ASSERT_EQ(FE_ALL_EXCEPT, all_flags);
#endif
}

// TODO(bugs.webrtc.org/8948): NDEBUG is not reliable on downstream projects,
// keep false positive/negative tests disabled until fixed.
// #ifdef NDEBUG
#define MAYBE_CheckNoFalsePositives DISABLED_CheckNoFalsePositives
#define MAYBE_CheckNoFalseNegatives DISABLED_CheckNoFalseNegatives
// #else
// #define MAYBE_CheckNoFalsePositives CheckNoFalsePositives
// #define MAYBE_CheckNoFalseNegatives CheckNoFalseNegatives
// #endif

// The floating point exception observer only works in debug mode.
// Trigger each single floating point exception while observing all the other
// exceptions. It must not fail.
TEST(FloatingPointExceptionObserverTest, MAYBE_CheckNoFalsePositives) {
  for (const auto exception_code : GetExceptionCodes()) {
    SCOPED_TRACE(exception_code.second);
    const int trigger = exception_code.first;
    int observe = FE_ALL_EXCEPT & ~trigger;
    // Over/underflows also trigger FE_INEXACT; hence, ignore FE_INEXACT (which
    // would be a false positive).
    if (trigger & (FE_OVERFLOW | FE_UNDERFLOW))
      observe &= ~FE_INEXACT;
    TriggerObserveFloatingPointExceptions(trigger, observe);
  }
}

// Trigger each single floating point exception while observing it. Check that
// this fails.
TEST(FloatingPointExceptionObserverTest, MAYBE_CheckNoFalseNegatives) {
  for (const auto exception_code : GetExceptionCodes()) {
    SCOPED_TRACE(exception_code.second);
    const int trigger = exception_code.first;
#ifdef WEBRTC_ANDROID
    // TODO(bugs.webrtc.org/8948): FE_INEXACT is not triggered on Android.
    if (trigger == FE_INEXACT)
      continue;
#endif
    EXPECT_NONFATAL_FAILURE(
        TriggerObserveFloatingPointExceptions(trigger, trigger), "");
  }
}

}  // namespace test
}  // namespace webrtc
