/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_RANDOM_H_
#define WEBRTC_TEST_RANDOM_H_

#include <limits>

#include "webrtc/typedefs.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

namespace test {

class Random {
 public:
  explicit Random(uint32_t seed);

  // Return pseudo-random integer of the specified type.
  template <typename T>
  T Rand() {
    static_assert(std::numeric_limits<T>::is_integer &&
                      std::numeric_limits<T>::radix == 2 &&
                      std::numeric_limits<T>::digits <= 32,
                  "Rand is only supported for built-in integer types that are "
                  "32 bits or smaller.");
    return static_cast<T>(Rand(std::numeric_limits<uint32_t>::max()));
  }

  // Uniformly distributed pseudo-random number in the interval [0, t].
  uint32_t Rand(uint32_t t);

  // Uniformly distributed pseudo-random number in the interval [low, high].
  uint32_t Rand(uint32_t low, uint32_t high);

  // Normal Distribution.
  int Gaussian(int mean, int standard_deviation);

  // Exponential Distribution.
  int Exponential(float lambda);

  // TODO(solenberg): Random from histogram.
  // template<typename T> int Distribution(const std::vector<T> histogram) {

 private:
  uint32_t a_;
  uint32_t b_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Random);
};

// Return pseudo-random number in the interval [0.0, 1.0).
template <>
float Random::Rand<float>();

// Return pseudo-random boolean value.
template <>
bool Random::Rand<bool>();

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_RANDOM_H_
