/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_

#include <complex>
#include <memory>

namespace webrtc {

namespace intelligibility {

// Internal helper for computing the power of a stream of arrays.
// The result is an array of power per position: the i-th power is the power of
// the stream of data on the i-th positions in the input arrays.
class PowerEstimator {
 public:
  // Construct an instance for the given input array length (|freqs|), with the
  // appropriate parameters. |decay| is the forgetting factor.
  PowerEstimator(size_t freqs, float decay);

  // Add a new data point to the series.
  void Step(const std::complex<float>* data);

  // The current power array.
  const float* Power();

 private:
  // TODO(ekmeyerson): Switch the following running means
  // and histories from std::unique_ptr to std::vector.
  std::unique_ptr<std::complex<float>[]> running_mean_sq_;

  // The current magnitude array.
  std::unique_ptr<float[]> magnitude_;
  // The current power array.
  std::unique_ptr<float[]> power_;

  const size_t num_freqs_;
  const float decay_;
};

// Helper class for smoothing gain changes. On each application step, the
// currently used gains are changed towards a set of settable target gains,
// constrained by a limit on the magnitude of the changes.
class GainApplier {
 public:
  GainApplier(size_t freqs, float change_limit);

  // Copy |in_block| to |out_block|, multiplied by the current set of gains,
  // and step the current set of gains towards the target set.
  void Apply(const std::complex<float>* in_block,
             std::complex<float>* out_block);

  // Return the current target gain set. Modify this array to set the targets.
  float* target() const { return target_.get(); }

 private:
  const size_t num_freqs_;
  const float change_limit_;
  std::unique_ptr<float[]> target_;
  std::unique_ptr<float[]> current_;
};

}  // namespace intelligibility

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
