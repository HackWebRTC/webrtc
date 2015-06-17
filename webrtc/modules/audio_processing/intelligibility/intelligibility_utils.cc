/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using std::complex;

namespace {

// Return |current| changed towards |target|, with the change being at most
// |limit|.
inline float UpdateFactor(float target, float current, float limit) {
  float delta = fabsf(target - current);
  float sign = copysign(1.0f, target - current);
  return current + sign * fminf(delta, limit);
}

// std::isfinite for complex numbers.
inline bool cplxfinite(complex<float> c) {
  return std::isfinite(c.real()) && std::isfinite(c.imag());
}

// std::isnormal for complex numbers.
inline bool cplxnormal(complex<float> c) {
  return std::isnormal(c.real()) && std::isnormal(c.imag());
}

// Apply a small fudge to degenerate complex values. The numbers in the array
// were chosen randomly, so that even a series of all zeroes has some small
// variability.
inline complex<float> zerofudge(complex<float> c) {
  const static complex<float> fudge[7] = {
    {0.001f, 0.002f}, {0.008f, 0.001f}, {0.003f, 0.008f}, {0.0006f, 0.0009f},
    {0.001f, 0.004f}, {0.003f, 0.004f}, {0.002f, 0.009f}
  };
  static int fudge_index = 0;
  if (cplxfinite(c) && !cplxnormal(c)) {
    fudge_index = (fudge_index + 1) % 7;
    return c + fudge[fudge_index];
  }
  return c;
}

// Incremental mean computation. Return the mean of the series with the
// mean |mean| with added |data|.
inline complex<float> NewMean(complex<float> mean, complex<float> data,
                                 int count) {
  return mean + (data - mean) / static_cast<float>(count);
}

inline void AddToMean(complex<float> data, int count, complex<float>* mean) {
  (*mean) = NewMean(*mean, data, count);
}

}  // namespace

using std::min;

namespace webrtc {

namespace intelligibility {

static const int kWindowBlockSize = 10;

VarianceArray::VarianceArray(int freqs, StepType type, int window_size,
                             float decay)
    : running_mean_(new complex<float>[freqs]()),
      running_mean_sq_(new complex<float>[freqs]()),
      sub_running_mean_(new complex<float>[freqs]()),
      sub_running_mean_sq_(new complex<float>[freqs]()),
      variance_(new float[freqs]()),
      conj_sum_(new float[freqs]()),
      freqs_(freqs),
      window_size_(window_size),
      decay_(decay),
      history_cursor_(0),
      count_(0),
      array_mean_(0.0f) {
  history_.reset(new scoped_ptr<complex<float>[]>[freqs_]());
  for (int i = 0; i < freqs_; ++i) {
    history_[i].reset(new complex<float>[window_size_]());
  }
  subhistory_.reset(new scoped_ptr<complex<float>[]>[freqs_]());
  for (int i = 0; i < freqs_; ++i) {
    subhistory_[i].reset(new complex<float>[window_size_]());
  }
  subhistory_sq_.reset(new scoped_ptr<complex<float>[]>[freqs_]());
  for (int i = 0; i < freqs_; ++i) {
    subhistory_sq_[i].reset(new complex<float>[window_size_]());
  }
  switch (type) {
    case kStepInfinite:
      step_func_ = &VarianceArray::InfiniteStep;
      break;
    case kStepDecaying:
      step_func_ = &VarianceArray::DecayStep;
      break;
    case kStepWindowed:
      step_func_ = &VarianceArray::WindowedStep;
      break;
    case kStepBlocked:
      step_func_ = &VarianceArray::BlockedStep;
      break;
  }
}

// Compute the variance with Welford's algorithm, adding some fudge to
// the input in case of all-zeroes.
void VarianceArray::InfiniteStep(const complex<float>* data, bool skip_fudge) {
  array_mean_ = 0.0f;
  ++count_;
  for (int i = 0; i < freqs_; ++i) {
    complex<float> sample = data[i];
    if (!skip_fudge) {
      sample = zerofudge(sample);
    }
    if (count_ == 1) {
      running_mean_[i] = sample;
      variance_[i] = 0.0f;
    } else {
      float old_sum = conj_sum_[i];
      complex<float> old_mean = running_mean_[i];
      running_mean_[i] = old_mean + (sample - old_mean) /
          static_cast<float>(count_);
      conj_sum_[i] = (old_sum + std::conj(sample - old_mean) *
          (sample - running_mean_[i])).real();
      variance_[i] = conj_sum_[i] / (count_ - 1); // + fudge[fudge_index].real();
      if (skip_fudge && false) {
        //variance_[i] -= fudge[fudge_index].real();
      }
    }
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

// Compute the variance from the beginning, with exponential decaying of the
// series data.
void VarianceArray::DecayStep(const complex<float>* data, bool /*dummy*/) {
  array_mean_ = 0.0f;
  ++count_;
  for (int i = 0; i < freqs_; ++i) {
    complex<float> sample = data[i];
    sample = zerofudge(sample);

    if (count_ == 1) {
      running_mean_[i] = sample;
      running_mean_sq_[i] = sample * std::conj(sample);
      variance_[i] = 0.0f;
    } else {
      complex<float> prev = running_mean_[i];
      complex<float> prev2 = running_mean_sq_[i];
      running_mean_[i] = decay_ * prev + (1.0f - decay_) * sample;
      running_mean_sq_[i] = decay_ * prev2 +
        (1.0f - decay_) * sample * std::conj(sample);
      //variance_[i] = decay_ * variance_[i] + (1.0f - decay_) * (
      //  (sample - running_mean_[i]) * std::conj(sample - running_mean_[i])).real();
      variance_[i] = (running_mean_sq_[i] - running_mean_[i] * std::conj(running_mean_[i])).real();
    }

    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

// Windowed variance computation. On each step, the variances for the
// window are recomputed from scratch, using Welford's algorithm.
void VarianceArray::WindowedStep(const complex<float>* data, bool /*dummy*/) {
  int num = min(count_ + 1, window_size_);
  array_mean_ = 0.0f;
  for (int i = 0; i < freqs_; ++i) {
    complex<float> mean;
    float conj_sum = 0.0f;

    history_[i][history_cursor_] = data[i];

    mean = history_[i][history_cursor_];
    variance_[i] = 0.0f;
    for (int j = 1; j < num; ++j) {
      complex<float> sample = zerofudge(
          history_[i][(history_cursor_ + j) % window_size_]);
      sample = history_[i][(history_cursor_ + j) % window_size_];
      float old_sum = conj_sum;
      complex<float> old_mean = mean;

      mean = old_mean + (sample - old_mean) / static_cast<float>(j + 1);
      conj_sum = (old_sum + std::conj(sample - old_mean) *
                                     (sample - mean)).real();
      variance_[i] = conj_sum / (j);
    }
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
  history_cursor_ = (history_cursor_ + 1) % window_size_;
  ++count_;
}

// Variance with a window of blocks. Within each block, the variances are
// recomputed from scratch at every stp, using |Var(X) = E(X^2) - E^2(X)|.
// Once a block is filled with kWindowBlockSize samples, it is added to the
// history window and a new block is started. The variances for the window
// are recomputed from scratch at each of these transitions.
void VarianceArray::BlockedStep(const complex<float>* data, bool /*dummy*/) {
  int blocks = min(window_size_, history_cursor_);
  for (int i = 0; i < freqs_; ++i) {
    AddToMean(data[i], count_ + 1, &sub_running_mean_[i]);
    AddToMean(data[i] * std::conj(data[i]), count_ + 1,
              &sub_running_mean_sq_[i]);
    subhistory_[i][history_cursor_ % window_size_] = sub_running_mean_[i];
    subhistory_sq_[i][history_cursor_ % window_size_] = sub_running_mean_sq_[i];

    variance_[i] = (NewMean(running_mean_sq_[i], sub_running_mean_sq_[i],
                            blocks) -
                   NewMean(running_mean_[i], sub_running_mean_[i], blocks) *
                   std::conj(NewMean(running_mean_[i], sub_running_mean_[i],
                                     blocks))).real();
    if (count_ == kWindowBlockSize - 1) {
      sub_running_mean_[i] = complex<float>(0.0f, 0.0f);
      sub_running_mean_sq_[i] = complex<float>(0.0f, 0.0f);
      running_mean_[i] = complex<float>(0.0f, 0.0f);
      running_mean_sq_[i] = complex<float>(0.0f, 0.0f);
      for (int j = 0; j < min(window_size_, history_cursor_); ++j) {
        AddToMean(subhistory_[i][j], j, &running_mean_[i]);
        AddToMean(subhistory_sq_[i][j], j, &running_mean_sq_[i]);
      }
      ++history_cursor_;
    }
  }
  ++count_;
  if (count_ == kWindowBlockSize) {
    count_ = 0;
  }
}

void VarianceArray::Clear() {
  memset(running_mean_.get(), 0, sizeof(*running_mean_.get()) * freqs_);
  memset(running_mean_sq_.get(), 0, sizeof(*running_mean_sq_.get()) * freqs_);
  memset(variance_.get(), 0, sizeof(*variance_.get()) * freqs_);
  memset(conj_sum_.get(), 0, sizeof(*conj_sum_.get()) * freqs_);
  history_cursor_ = 0;
  count_ = 0;
  array_mean_ = 0.0f;
}

void VarianceArray::ApplyScale(float scale) {
  array_mean_ = 0.0f;
  for (int i = 0; i < freqs_; ++i) {
    variance_[i] *= scale * scale;
    array_mean_ += (variance_[i] - array_mean_) / (i + 1);
  }
}

GainApplier::GainApplier(int freqs, float change_limit)
    : freqs_(freqs),
      change_limit_(change_limit),
      target_(new float[freqs]()),
      current_(new float[freqs]()) {
  for (int i = 0; i < freqs; ++i) {
    target_[i] = 1.0f;
    current_[i] = 1.0f;
  }
}

void GainApplier::Apply(const complex<float>* in_block,
                        complex<float>* out_block) {
  for (int i = 0; i < freqs_; ++i) {
    float factor = sqrtf(fabsf(current_[i]));
    if (!std::isnormal(factor)) {
      factor = 1.0f;
    }
    out_block[i] = factor * in_block[i];
    current_[i] = UpdateFactor(target_[i], current_[i], change_limit_);
  }
}

}  // namespace intelligibility

}  // namespace webrtc

