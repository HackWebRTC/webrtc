// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Input buffer layout, dividing the total buffer into regions (r0_ - r5_):
//
// |----------------|-----------------------------------------|----------------|
//
//                                   kBlockSize + kKernelSize / 2
//                   <--------------------------------------------------------->
//                                              r0_
//
//  kKernelSize / 2   kKernelSize / 2         kKernelSize / 2   kKernelSize / 2
// <---------------> <--------------->       <---------------> <--------------->
//        r1_               r2_                     r3_               r4_
//
//                                                     kBlockSize
//                                     <--------------------------------------->
//                                                        r5_
//
// The algorithm:
//
// 1) Consume input frames into r0_ (r1_ is zero-initialized).
// 2) Position kernel centered at start of r0_ (r2_) and generate output frames
//    until kernel is centered at start of r4_ or we've finished generating all
//    the output frames.
// 3) Copy r3_ to r1_ and r4_ to r2_.
// 4) Consume input frames into r5_ (zero-pad if we run out of input).
// 5) Goto (2) until all of input is consumed.
//
// Note: we're glossing over how the sub-sample handling works with
// |virtual_source_idx_|, etc.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "media/base/sinc_resampler.h"

#include <cmath>

#include "base/cpu.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
#include <xmmintrin.h>
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media {

namespace {

enum {
  // The kernel size can be adjusted for quality (higher is better) at the
  // expense of performance.  Must be a multiple of 32.
  // TODO(dalecurtis): Test performance to see if we can jack this up to 64+.
  kKernelSize = 32,

  // The number of destination frames generated per processing pass.  Affects
  // how often and for how much SincResampler calls back for input.  Must be
  // greater than kKernelSize.
  kBlockSize = 512,

  // The kernel offset count is used for interpolation and is the number of
  // sub-sample kernel shifts.  Can be adjusted for quality (higher is better)
  // at the expense of allocating more memory.
  kKernelOffsetCount = 32,
  kKernelStorageSize = kKernelSize * (kKernelOffsetCount + 1),

  // The size (in samples) of the internal buffer used by the resampler.
  kBufferSize = kBlockSize + kKernelSize
};

}  // namespace

const int SincResampler::kMaximumLookAheadSize = kBufferSize;

SincResampler::SincResampler(double io_sample_rate_ratio, const ReadCB& read_cb)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      virtual_source_idx_(0),
      buffer_primed_(false),
      read_cb_(read_cb),
      // Create input buffers with a 16-byte alignment for SSE optimizations.
      kernel_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kKernelStorageSize, 16))),
      input_buffer_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kBufferSize, 16))),
      // Setup various region pointers in the buffer (see diagram above).
      r0_(input_buffer_.get() + kKernelSize / 2),
      r1_(input_buffer_.get()),
      r2_(r0_),
      r3_(r0_ + kBlockSize - kKernelSize / 2),
      r4_(r0_ + kBlockSize),
      r5_(r0_ + kKernelSize / 2) {
  // Ensure kKernelSize is a multiple of 32 for easy SSE optimizations; causes
  // r0_ and r5_ (used for input) to always be 16-byte aligned by virtue of
  // input_buffer_ being 16-byte aligned.
  DCHECK_EQ(kKernelSize % 32, 0) << "kKernelSize must be a multiple of 32!";
  DCHECK_GT(kBlockSize, kKernelSize)
      << "kBlockSize must be greater than kKernelSize!";
  // Basic sanity checks to ensure buffer regions are laid out correctly:
  // r0_ and r2_ should always be the same position.
  DCHECK_EQ(r0_, r2_);
  // r1_ at the beginning of the buffer.
  DCHECK_EQ(r1_, input_buffer_.get());
  // r1_ left of r2_, r2_ left of r5_ and r1_, r2_ size correct.
  DCHECK_EQ(r2_ - r1_, r5_ - r2_);
  // r3_ left of r4_, r5_ left of r0_ and r3_ size correct.
  DCHECK_EQ(r4_ - r3_, r5_ - r0_);
  // r3_, r4_ size correct and r4_ at the end of the buffer.
  DCHECK_EQ(r4_ + (r4_ - r3_), r1_ + kBufferSize);
  // r5_ size correct and at the end of the buffer.
  DCHECK_EQ(r5_ + kBlockSize, r1_ + kBufferSize);

  memset(kernel_storage_.get(), 0,
         sizeof(*kernel_storage_.get()) * kKernelStorageSize);
  memset(input_buffer_.get(), 0, sizeof(*input_buffer_.get()) * kBufferSize);

  InitializeKernel();
}

SincResampler::~SincResampler() {}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  static const double kAlpha = 0.16;
  static const double kA0 = 0.5 * (1.0 - kAlpha);
  static const double kA1 = 0.5;
  static const double kA2 = 0.5 * kAlpha;

  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor =
      io_sample_rate_ratio_ > 1.0 ? 1.0 / io_sample_rate_ratio_ : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // TODO(crogers): this value is empirical and to be more exact should vary
  // depending on kKernelSize.
  sinc_scale_factor *= 0.9;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    double subsample_offset =
        static_cast<double>(offset_idx) / kKernelOffsetCount;

    for (int i = 0; i < kKernelSize; ++i) {
      // Compute the sinc with offset.
      double s =
          sinc_scale_factor * M_PI * (i - kKernelSize / 2 - subsample_offset);
      double sinc = (!s ? 1.0 : sin(s) / s) * sinc_scale_factor;

      // Compute Blackman window, matching the offset of the sinc().
      double x = (i - subsample_offset) / kKernelSize;
      double window = kA0 - kA1 * cos(2.0 * M_PI * x) + kA2
          * cos(4.0 * M_PI * x);

      // Window the sinc() function and store at the correct offset.
      kernel_storage_.get()[i + offset_idx * kKernelSize] = sinc * window;
    }
  }
}

void SincResampler::Resample(float* destination, int frames) {
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_) {
    read_cb_.Run(r0_, kBlockSize + kKernelSize / 2);
    buffer_primed_ = true;
  }

  // Step (2) -- Resample!
  while (remaining_frames) {
    while (virtual_source_idx_ < kBlockSize) {
      // |virtual_source_idx_| lies in between two kernel offsets so figure out
      // what they are.
      int source_idx = static_cast<int>(virtual_source_idx_);
      double subsample_remainder = virtual_source_idx_ - source_idx;

      double virtual_offset_idx = subsample_remainder * kKernelOffsetCount;
      int offset_idx = static_cast<int>(virtual_offset_idx);

      // We'll compute "convolutions" for the two kernels which straddle
      // |virtual_source_idx_|.
      float* k1 = kernel_storage_.get() + offset_idx * kKernelSize;
      float* k2 = k1 + kKernelSize;

      // Initialize input pointer based on quantized |virtual_source_idx_|.
      float* input_ptr = r1_ + source_idx;

      // Figure out how much to weight each kernel's "convolution".
      double kernel_interpolation_factor = virtual_offset_idx - offset_idx;
      *destination++ = Convolve(
          input_ptr, k1, k2, kernel_interpolation_factor);

      // Advance the virtual index.
      virtual_source_idx_ += io_sample_rate_ratio_;

      if (!--remaining_frames)
        return;
    }

    // Wrap back around to the start.
    virtual_source_idx_ -= kBlockSize;

    // Step (3) Copy r3_ to r1_ and r4_ to r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));
    memcpy(r2_, r4_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));

    // Step (4)
    // Refresh the buffer with more input.
    read_cb_.Run(r5_, kBlockSize);
  }
}

int SincResampler::ChunkSize() {
  return kBlockSize / io_sample_rate_ratio_;
}

void SincResampler::Flush() {
  virtual_source_idx_ = 0;
  buffer_primed_ = false;
  memset(input_buffer_.get(), 0, sizeof(*input_buffer_.get()) * kBufferSize);
}

float SincResampler::Convolve(const float* input_ptr, const float* k1,
                              const float* k2,
                              double kernel_interpolation_factor) {
  // Rely on function level static initialization to keep ConvolveProc selection
  // thread safe.
  typedef float (*ConvolveProc)(const float* src, const float* k1,
                                const float* k2,
                                double kernel_interpolation_factor);
#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
  static const ConvolveProc kConvolveProc =
      base::CPU().has_sse() ? Convolve_SSE : Convolve_C;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  static const ConvolveProc kConvolveProc = Convolve_NEON;
#else
  static const ConvolveProc kConvolveProc = Convolve_C;
#endif

  return kConvolveProc(input_ptr, k1, k2, kernel_interpolation_factor);
}

float SincResampler::Convolve_C(const float* input_ptr, const float* k1,
                                const float* k2,
                                double kernel_interpolation_factor) {
  float sum1 = 0;
  float sum2 = 0;

  // Generate a single output sample.  Unrolling this loop hurt performance in
  // local testing.
  int n = kKernelSize;
  while (n--) {
    sum1 += *input_ptr * *k1++;
    sum2 += *input_ptr++ * *k2++;
  }

  // Linearly interpolate the two "convolutions".
  return (1.0 - kernel_interpolation_factor) * sum1
      + kernel_interpolation_factor * sum2;
}

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
float SincResampler::Convolve_SSE(const float* input_ptr, const float* k1,
                                  const float* k2,
                                  double kernel_interpolation_factor) {
  // Ensure |k1|, |k2| are 16-byte aligned for SSE usage.  Should always be true
  // so long as kKernelSize is a multiple of 16.
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k1) & 0x0F);
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k2) & 0x0F);

  __m128 m_input;
  __m128 m_sums1 = _mm_setzero_ps();
  __m128 m_sums2 = _mm_setzero_ps();

  // Based on |input_ptr| alignment, we need to use loadu or load.  Unrolling
  // these loops hurt performance in local testing.
  if (reinterpret_cast<uintptr_t>(input_ptr) & 0x0F) {
    for (int i = 0; i < kKernelSize; i += 4) {
      m_input = _mm_loadu_ps(input_ptr + i);
      m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
      m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
    }
  } else {
    for (int i = 0; i < kKernelSize; i += 4) {
      m_input = _mm_load_ps(input_ptr + i);
      m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
      m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
    }
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = _mm_mul_ps(m_sums1, _mm_set_ps1(1.0 - kernel_interpolation_factor));
  m_sums2 = _mm_mul_ps(m_sums2, _mm_set_ps1(kernel_interpolation_factor));
  m_sums1 = _mm_add_ps(m_sums1, m_sums2);

  // Sum components together.
  float result;
  m_sums2 = _mm_add_ps(_mm_movehl_ps(m_sums1, m_sums1), m_sums1);
  _mm_store_ss(&result, _mm_add_ss(m_sums2, _mm_shuffle_ps(
      m_sums2, m_sums2, 1)));

  return result;
}
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
float SincResampler::Convolve_NEON(const float* input_ptr, const float* k1,
                                   const float* k2,
                                   double kernel_interpolation_factor) {
  float32x4_t m_input;
  float32x4_t m_sums1 = vmovq_n_f32(0);
  float32x4_t m_sums2 = vmovq_n_f32(0);

  const float* upper = input_ptr + kKernelSize;
  for (; input_ptr < upper; ) {
    m_input = vld1q_f32(input_ptr);
    input_ptr += 4;
    m_sums1 = vmlaq_f32(m_sums1, m_input, vld1q_f32(k1));
    k1 += 4;
    m_sums2 = vmlaq_f32(m_sums2, m_input, vld1q_f32(k2));
    k2 += 4;
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = vmlaq_f32(
      vmulq_f32(m_sums1, vmovq_n_f32(1.0 - kernel_interpolation_factor)),
      m_sums2, vmovq_n_f32(kernel_interpolation_factor));

  // Sum components together.
  float32x2_t m_half = vadd_f32(vget_high_f32(m_sums1), vget_low_f32(m_sums1));
  return vget_lane_f32(vpadd_f32(m_half, m_half), 0);
}
#endif

}  // namespace media
