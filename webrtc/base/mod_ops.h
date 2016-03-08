/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_MOD_OPS_H_
#define WEBRTC_BASE_MOD_OPS_H_

#include <limits>
#include <type_traits>

#include "webrtc/base/checks.h"

#define MOD_OPS_ASSERT_TYPE_IS_UNSIGNED(T)              \
  static_assert(std::numeric_limits<T>::is_integer &&   \
                    !std::numeric_limits<T>::is_signed, \
                "Type must be of unsigned integer.")

namespace webrtc {

template <unsigned long M>                                    // NOLINT
inline unsigned long Add(unsigned long a, unsigned long b) {  // NOLINT
  RTC_DCHECK_LT(a, M);
  unsigned long t = M - b % M;  // NOLINT
  unsigned long res = a - t;    // NOLINT
  if (t > a)
    return res + M;
  return res;
}

template <unsigned long M>                                         // NOLINT
inline unsigned long Subtract(unsigned long a, unsigned long b) {  // NOLINT
  RTC_DCHECK_LT(a, M);
  unsigned long sub = b % M;  // NOLINT
  if (a < sub)
    return M - (sub - a);
  return a - sub;
}

// Calculates the forward difference between two numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ForwardDiff(x, y) == 4
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          |----->----->----->----->----->
//
// ForwardDiff(y, x) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// -->----->                              |----->---
//
template <typename T>
inline T ForwardDiff(T a, T b) {
  MOD_OPS_ASSERT_TYPE_IS_UNSIGNED(T);
  return b - a;
}

// Calculates the reverse difference between two numbers.
//
// Example:
// uint8_t x = 253;
// uint8_t y = 2;
//
// ReverseDiff(y, x) == 5
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
//          <-----<-----<-----<-----<-----|
//
// ReverseDiff(x, y) == 251
//
//   252   253   254   255    0     1     2     3
// #################################################
// |     |  x  |     |     |     |     |  y  |     |
// #################################################
// ---<-----|                             |<-----<--
//
template <typename T>
inline T ReverseDiff(T a, T b) {
  MOD_OPS_ASSERT_TYPE_IS_UNSIGNED(T);
  return a - b;
}

// Test if the sequence number a is ahead or at sequence number b.
// If the two sequence numbers are at max distance from each other
// then the sequence number with highest value is considered to
// be ahead.
template <typename T>
inline bool AheadOrAt(T a, T b) {
  MOD_OPS_ASSERT_TYPE_IS_UNSIGNED(T);
  const T maxDist = std::numeric_limits<T>::max() / 2 + T(1);
  if (a - b == maxDist)
    return b < a;
  return ForwardDiff(b, a) < maxDist;
}

// Test if sequence number a is ahead of sequence number b.
template <typename T>
inline bool AheadOf(T a, T b) {
  MOD_OPS_ASSERT_TYPE_IS_UNSIGNED(T);
  return a != b && AheadOrAt(a, b);
}

}  // namespace webrtc

#endif  // WEBRTC_BASE_MOD_OPS_H_
