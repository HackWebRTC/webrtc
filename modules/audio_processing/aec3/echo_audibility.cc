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

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"

namespace webrtc {

namespace {
constexpr int kUnitializedIndex = -1;
}  // namespace

EchoAudibility::EchoAudibility() {
  Reset();
}

void EchoAudibility::Reset() {
  render_stationarity_.Reset();
  render_write_prev_ = kUnitializedIndex;
}

EchoAudibility::~EchoAudibility() = default;

void EchoAudibility::UpdateRenderStationarityFlags(
    const RenderBuffer& render_buffer,
    size_t delay_blocks) {
  int idx_at_delay =
      render_buffer.OffsetSpectrumIndex(render_buffer.Position(), delay_blocks);
  size_t num_lookahead = render_buffer.Headroom() - delay_blocks + 1;
  render_stationarity_.UpdateStationarityFlags(
      render_buffer.GetSpectrumBuffer(), idx_at_delay, num_lookahead);
}

void EchoAudibility::UpdateRenderNoiseEstimator(
    const RenderBuffer& render_buffer) {
  if (render_write_prev_ == kUnitializedIndex) {
    render_write_prev_ = render_buffer.GetWritePositionSpectrum();
    return;
  }
  int render_write_current = render_buffer.GetWritePositionSpectrum();

  for (int idx = render_write_prev_; idx != render_write_current;
       idx = render_buffer.DecIdx(idx)) {
    render_stationarity_.UpdateNoiseEstimator(
        render_buffer.SpectrumAtIndex(idx));
  }

  render_write_prev_ = render_write_current;
}

void EchoAudibility::Update(const RenderBuffer& render_buffer,
                            size_t delay_blocks,
                            size_t capture_block_counter,
                            bool external_delay_seen) {
  UpdateRenderNoiseEstimator(render_buffer);

  if (external_delay_seen) {
    UpdateRenderStationarityFlags(render_buffer, delay_blocks);
  }
}

}  // namespace webrtc
