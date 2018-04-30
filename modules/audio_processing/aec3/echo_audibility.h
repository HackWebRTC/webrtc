/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

class EchoAudibility {
 public:
  EchoAudibility();
  ~EchoAudibility();

  // Feed new render data to the echo audibility estimator.
  void Update(const RenderBuffer& render_buffer,
              size_t delay_blocks,
              size_t capture_block_counter_,
              bool external_delay_seen);

  // Get the residual echo scaling.
  void GetResidualEchoScaling(rtc::ArrayView<float> residual_scaling) const {
    for (size_t band = 0; band < residual_scaling.size(); ++band) {
      if (render_stationarity_.IsBandStationary(band)) {
        residual_scaling[band] = 0.f;
      } else {
        residual_scaling[band] = 1.0f;
      }
    }
  }

 private:
  // Reset the EchoAudibility class.
  void Reset();

  // Compute the residual scaling per frequency for the current frame.
  void ComputeResidualScaling();

  // Updates the render stationarity flags for the current frame.
  void UpdateRenderStationarityFlags(const RenderBuffer& render_buffer,
                                     size_t delay_blocks);

  // Updates the noise estimator with the new render data since the previous
  // call to this method.
  void UpdateRenderNoiseEstimator(const RenderBuffer& render_buffer);

  int render_write_prev_;
  StationarityEstimator render_stationarity_;
  RTC_DISALLOW_COPY_AND_ASSIGN(EchoAudibility);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
