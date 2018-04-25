/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_audibility.h"

#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"

namespace webrtc {

EchoAudibility::EchoAudibility() {
  Reset();
}

void EchoAudibility::Reset() {
  render_stationarity_.Reset();
}

EchoAudibility::~EchoAudibility() = default;

void EchoAudibility::Update(const RenderBuffer& render_buffer,
                            size_t delay_blocks,
                            size_t capture_block_counter) {
  RTC_DCHECK_GT(capture_block_counter, delay_blocks);

  size_t num_lookahead = std::min(StationarityEstimator::GetMaxNumLookAhead(),
                                  render_buffer.Headroom() - delay_blocks + 1);
  int render_block_number = capture_block_counter - delay_blocks;

  for (size_t k = 0; k < (num_lookahead + 1); ++k) {
    // Delay changes can potentially make that not all the farend blocks
    // are seen. That effect is assumed to have a minimum effect in the
    // estimation.
    render_stationarity_.Update(render_buffer.Spectrum(delay_blocks - k),
                                render_block_number + k);
  }
  render_stationarity_.UpdateStationarityFlags(render_block_number,
                                               num_lookahead);
}

}  // namespace webrtc
