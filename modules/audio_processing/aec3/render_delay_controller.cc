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
  void SetDelay(size_t render_delay) override;
  rtc::Optional<size_t> GetDelay(const DownsampledRenderBuffer& render_buffer,
                                 rtc::ArrayView<const float> capture) override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  rtc::Optional<size_t> delay_;
  EchoPathDelayEstimator delay_estimator_;
  size_t align_call_counter_ = 0;
  std::vector<float> delay_buf_;
  int delay_buf_index_ = 0;
  RenderDelayControllerMetrics metrics_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayControllerImpl);
};

size_t ComputeNewBufferDelay(rtc::Optional<size_t> current_delay,
                             size_t delay_samples) {
  // The below division is not exact and the truncation is intended.
  const int echo_path_delay_blocks = delay_samples >> kBlockSizeLog2;
  constexpr int kDelayHeadroomBlocks = 1;

  // Compute the buffer delay increase required to achieve the desired latency.
  size_t new_delay = std::max(echo_path_delay_blocks - kDelayHeadroomBlocks, 0);

  // Add hysteresis.
  if (current_delay) {
    if (new_delay == *current_delay + 1) {
      new_delay = *current_delay;
    }
  }

  return new_delay;
}

int RenderDelayControllerImpl::instance_count_ = 0;

RenderDelayControllerImpl::RenderDelayControllerImpl(
    const EchoCanceller3Config& config,
    int non_causal_offset,
    int sample_rate_hz)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      delay_estimator_(data_dumper_.get(), config),
      delay_buf_(kBlockSize * non_causal_offset, 0.f) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz));
  delay_estimator_.LogDelayEstimationProperties(sample_rate_hz,
                                                delay_buf_.size());
}

RenderDelayControllerImpl::~RenderDelayControllerImpl() = default;

void RenderDelayControllerImpl::Reset() {
  delay_ = rtc::nullopt;
  align_call_counter_ = 0;
  std::fill(delay_buf_.begin(), delay_buf_.end(), 0.f);
  delay_estimator_.Reset();
}

void RenderDelayControllerImpl::SetDelay(size_t render_delay) {
  if (delay_ != render_delay) {
    // If a the delay set does not match the actual delay, reset the delay
    // controller.
    Reset();
    delay_ = render_delay;
  }
}

rtc::Optional<size_t> RenderDelayControllerImpl::GetDelay(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  ++align_call_counter_;

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
    if (align_call_counter_ > kNumBlocksPerSecond) {
      delay_ = ComputeNewBufferDelay(delay_, static_cast<int>(*delay_samples));
    }

    metrics_.Update(static_cast<int>(*delay_samples), delay_ ? *delay_ : 0);
  } else {
    metrics_.Update(rtc::nullopt, delay_ ? *delay_ : 0);
  }

  data_dumper_->DumpRaw("aec3_render_delay_controller_delay",
                        delay_samples ? *delay_samples : 0);
  data_dumper_->DumpRaw("aec3_render_delay_controller_buffer_delay",
                        delay_ ? *delay_ : 0);

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
