/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediastreaminterface.h"

namespace webrtc {

const char MediaStreamTrackInterface::kVideoKind[] = "video";
const char MediaStreamTrackInterface::kAudioKind[] = "audio";

// TODO(ivoc): Remove this when the function becomes pure virtual.
AudioProcessorInterface::AudioProcessorStatistics
AudioProcessorInterface::GetStats(bool /*has_remote_tracks*/) {
  AudioProcessorStats stats;
  GetStats(&stats);
  AudioProcessorStatistics new_stats;
  new_stats.aec_divergent_filter_fraction =
      rtc::Optional<double>(stats.aec_divergent_filter_fraction);
  new_stats.aec_quality_min = rtc::Optional<double>(stats.aec_quality_min);
  new_stats.echo_delay_median_ms =
      rtc::Optional<int32_t>(stats.echo_delay_median_ms);
  new_stats.echo_delay_std_ms = rtc::Optional<int32_t>(stats.echo_delay_std_ms);
  new_stats.echo_return_loss = rtc::Optional<double>(stats.echo_return_loss);
  new_stats.echo_return_loss_enhancement =
      rtc::Optional<double>(stats.echo_return_loss_enhancement);
  new_stats.residual_echo_likelihood =
      rtc::Optional<double>(stats.residual_echo_likelihood);
  new_stats.residual_echo_likelihood_recent_max =
      rtc::Optional<double>(stats.residual_echo_likelihood_recent_max);
  return new_stats;
}

}  // namespace webrtc
