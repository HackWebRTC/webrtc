/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/render_delay_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/echo_path_delay_estimator.h"
#include "modules/audio_processing/aec3/render_delay_controller_metrics.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

namespace {

class RenderDelayControllerImpl final : public RenderDelayController {
 public:
  RenderDelayControllerImpl(const EchoCanceller3Config& config,
                            int non_causal_offset,
                            int sample_rate_hz);
  ~RenderDelayControllerImpl() override;
  void Reset() override;
  rtc::Optional<DelayEstimate> GetDelay(
      const DownsampledRenderBuffer& render_buffer,
      rtc::ArrayView<const float> capture) override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const int delay_headroom_blocks_;
  const int hysteresis_limit_1_blocks_;
  const int hysteresis_limit_2_blocks_;
  rtc::Optional<DelayEstimate> delay_;
  EchoPathDelayEstimator delay_estimator_;
  std::vector<float> delay_buf_;
  int delay_buf_index_ = 0;
  RenderDelayControllerMetrics metrics_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayControllerImpl);
};

DelayEstimate ComputeNewBufferDelay(
    const rtc::Optional<DelayEstimate>& current_delay,
    int delay_headroom_blocks,
    int hysteresis_limit_1_blocks,
    int hysteresis_limit_2_blocks,
    DelayEstimate estimated_delay) {
  // The below division is not exact and the truncation is intended.
  const int echo_path_delay_blocks = estimated_delay.delay >> kBlockSizeLog2;

  // Compute the buffer delay increase required to achieve the desired latency.
  size_t new_delay_blocks =
      std::max(echo_path_delay_blocks - delay_headroom_blocks, 0);

  DelayEstimate new_delay(estimated_delay.quality, new_delay_blocks);

  // Add hysteresis.
  if (current_delay) {
    size_t current_delay_blocks = current_delay->delay;
    if (new_delay_blocks > current_delay_blocks) {
      if (new_delay_blocks <=
          current_delay_blocks + hysteresis_limit_1_blocks) {
        new_delay_blocks = current_delay_blocks;
      }
    } else if (new_delay_blocks < current_delay_blocks) {
      size_t hysteresis_limit = std::max(
          static_cast<int>(current_delay_blocks) - hysteresis_limit_2_blocks,
          0);
      if (new_delay_blocks >= hysteresis_limit) {
        new_delay_blocks = current_delay_blocks;
      }
    }
  }

  return DelayEstimate(estimated_delay.quality, new_delay_blocks);
}

int RenderDelayControllerImpl::instance_count_ = 0;

RenderDelayControllerImpl::RenderDelayControllerImpl(
    const EchoCanceller3Config& config,
    int non_causal_offset,
    int sample_rate_hz)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      delay_headroom_blocks_(
          static_cast<int>(config.delay.delay_headroom_blocks)),
      hysteresis_limit_1_blocks_(
          static_cast<int>(config.delay.hysteresis_limit_1_blocks)),
      hysteresis_limit_2_blocks_(
          static_cast<int>(config.delay.hysteresis_limit_2_blocks)),
      delay_estimator_(data_dumper_.get(), config),
      delay_buf_(kBlockSize * non_causal_offset, 0.f) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz));
  delay_estimator_.LogDelayEstimationProperties(sample_rate_hz,
                                                delay_buf_.size());
}

RenderDelayControllerImpl::~RenderDelayControllerImpl() = default;

void RenderDelayControllerImpl::Reset() {
  delay_ = rtc::nullopt;
  std::fill(delay_buf_.begin(), delay_buf_.end(), 0.f);
  delay_estimator_.Reset();
}

rtc::Optional<DelayEstimate> RenderDelayControllerImpl::GetDelay(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());

  // Estimate the delay with a delayed capture.
  RTC_DCHECK_LT(delay_buf_index_ + kBlockSize - 1, delay_buf_.size());
  rtc::ArrayView<const float> capture_delayed(&delay_buf_[delay_buf_index_],
                                              kBlockSize);
  auto delay_samples =
      delay_estimator_.EstimateDelay(render_buffer, capture_delayed);

  std::copy(capture.begin(), capture.end(),
            delay_buf_.begin() + delay_buf_index_);
  delay_buf_index_ = (delay_buf_index_ + kBlockSize) % delay_buf_.size();

  if (delay_samples) {
    // Compute and set new render delay buffer delay.

    delay_ = ComputeNewBufferDelay(delay_, delay_headroom_blocks_,
                                   hysteresis_limit_1_blocks_,
                                   hysteresis_limit_2_blocks_, *delay_samples);

    metrics_.Update(static_cast<int>(delay_samples->delay),
                    delay_ ? delay_->delay : 0);
  } else {
    metrics_.Update(rtc::nullopt, delay_ ? delay_->delay : 0);
  }

  data_dumper_->DumpRaw("aec3_render_delay_controller_delay",
                        delay_samples ? delay_samples->delay : 0);
  data_dumper_->DumpRaw("aec3_render_delay_controller_buffer_delay",
                        delay_ ? delay_->delay : 0);

  return delay_;
}

}  // namespace

RenderDelayController* RenderDelayController::Create(
    const EchoCanceller3Config& config,
    int non_causal_offset,
    int sample_rate_hz) {
  return new RenderDelayControllerImpl(config, non_causal_offset,
                                       sample_rate_hz);
}

}  // namespace webrtc
