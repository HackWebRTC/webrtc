/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_VECTOR_MATH_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_VECTOR_MATH_H_

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <math.h>
#include <algorithm>
#include <array>
#include <functional>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {
namespace aec3 {

// Provides optimizations for mathematical operations based on vectors.
class VectorMath {
 public:
  explicit VectorMath(Aec3Optimization optimization)
      : optimization_(optimization) {}

  // Elementwise square root.
  void Sqrt(rtc::ArrayView<float> x) {
    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2: {
        const int x_size = static_cast<int>(x.size());
        const int vector_limit = x_size >> 2;

        int j = 0;
        for (; j < vector_limit * 4; j += 4) {
          __m128 g = _mm_loadu_ps(&x[j]);
          g = _mm_sqrt_ps(g);
          _mm_storeu_ps(&x[j], g);
        }

        for (; j < x_size; ++j) {
          x[j] = sqrtf(x[j]);
        }
      } break;
#endif
      default:
        std::for_each(x.begin(), x.end(), [](float& a) { a = sqrtf(a); });
    }
  }

  // Elementwise vector multiplication z = x * y.
  void Multiply(rtc::ArrayView<const float> x,
                rtc::ArrayView<const float> y,
                rtc::ArrayView<float> z) {
    RTC_DCHECK_EQ(z.size(), x.size());
    RTC_DCHECK_EQ(z.size(), y.size());
    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2: {
        const int x_size = static_cast<int>(x.size());
        const int vector_limit = x_size >> 2;

        int j = 0;
        for (; j < vector_limit * 4; j += 4) {
          const __m128 x_j = _mm_loadu_ps(&x[j]);
          const __m128 y_j = _mm_loadu_ps(&y[j]);
          const __m128 z_j = _mm_mul_ps(x_j, y_j);
          _mm_storeu_ps(&z[j], z_j);
        }

        for (; j < x_size; ++j) {
          z[j] = x[j] * y[j];
        }
      } break;
#endif
      default:
        std::transform(x.begin(), x.end(), y.begin(), z.begin(),
                       std::multiplies<float>());
    }
  }

  // Elementwise vector accumulation z += x.
  void Accumulate(rtc::ArrayView<const float> x, rtc::ArrayView<float> z) {
    RTC_DCHECK_EQ(z.size(), x.size());
    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2: {
        const int x_size = static_cast<int>(x.size());
        const int vector_limit = x_size >> 2;

        int j = 0;
        for (; j < vector_limit * 4; j += 4) {
          const __m128 x_j = _mm_loadu_ps(&x[j]);
          __m128 z_j = _mm_loadu_ps(&z[j]);
          z_j = _mm_add_ps(x_j, z_j);
          _mm_storeu_ps(&z[j], z_j);
        }

        for (; j < x_size; ++j) {
          z[j] += x[j];
        }
      } break;
#endif
      default:
        std::transform(x.begin(), x.end(), z.begin(), z.begin(),
                       std::plus<float>());
    }
  }

 private:
  Aec3Optimization optimization_;
};

}  // namespace aec3

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_VECTOR_MATH_H_
