/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_RANDOM_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_RANDOM_H_

#include "webrtc/typedefs.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

class Random {
 public:
  explicit Random(uint32_t seed);

  // Return pseudo-random number in the interval [0.0, 1.0].
  float Rand();

  // Normal Distribution.
  int Gaussian(int mean, int standard_deviation);

  // TODO(solenberg): Random from histogram.
  // template<typename T> int Distribution(const std::vector<T> histogram) {

 private:
  uint32_t a_;
  uint32_t b_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Random);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_RANDOM_H_
