/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_RESAMPLER_INCLUDE_PUSH_RESAMPLER_H_
#define WEBRTC_COMMON_AUDIO_RESAMPLER_INCLUDE_PUSH_RESAMPLER_H_

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Resampler;
class PushSincResampler;

// Wraps the old resampler and new arbitrary rate conversion resampler. The
// old resampler will be used whenever it supports the requested rates, and
// otherwise the sinc resampler will be enabled.
class PushResampler {
 public:
  PushResampler();
  virtual ~PushResampler();

  // Must be called whenever the parameters change. Free to be called at any
  // time as it is a no-op if parameters have not changed since the last call.
  int InitializeIfNeeded(int src_sample_rate_hz, int dst_sample_rate_hz,
                         int num_channels);

  // Returns the total number of samples provided in destination (e.g. 32 kHz,
  // 2 channel audio gives 640 samples).
  int Resample(const int16_t* src, int src_length, int16_t* dst,
               int dst_capacity);

  bool use_sinc_resampler() const { return use_sinc_resampler_; }

 private:
  int ResampleSinc(const int16_t* src, int src_length, int16_t* dst,
                   int dst_capacity);

  scoped_ptr<Resampler> resampler_;
  scoped_ptr<PushSincResampler> sinc_resampler_;
  scoped_ptr<PushSincResampler> sinc_resampler_right_;
  int src_sample_rate_hz_;
  int dst_sample_rate_hz_;
  int num_channels_;
  bool use_sinc_resampler_;
  scoped_array<int16_t> src_left_;
  scoped_array<int16_t> src_right_;
  scoped_array<int16_t> dst_left_;
  scoped_array<int16_t> dst_right_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_RESAMPLER_INCLUDE_PUSH_RESAMPLER_H_
