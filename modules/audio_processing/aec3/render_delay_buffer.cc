/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_buffer.h"

#include <string.h>
#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kBufferHeadroom = kAdaptiveFilterLength;

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(const EchoCanceller3Config& config, size_t num_bands);
  ~RenderDelayBufferImpl() override;

  void Reset() override;
  BufferingEvent Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent PrepareCaptureCall() override;
  void SetDelay(size_t delay) override;
  size_t Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return blocks_.buffer.size() - 1 - kBufferHeadroom;
  }
  size_t MaxApiJitter() const override { return max_api_jitter_; }
  const RenderBuffer& GetRenderBuffer() const override {
    return echo_remover_buffer_;
  }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return low_rate_;
  }

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const size_t api_call_jitter_blocks_;
  const size_t min_echo_path_delay_blocks_;
  const int sub_block_size_;
  MatrixBuffer blocks_;
  VectorBuffer spectra_;
  FftBuffer ffts_;
  size_t delay_;
  int max_api_jitter_ = 0;
  int render_surplus_ = 0;
  bool first_reset_occurred_ = false;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer low_rate_;
  Decimator render_decimator_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  size_t capture_call_counter_ = 0;
  std::vector<float> render_ds_;
  int render_calls_in_a_row_ = 0;

  void UpdateBuffersWithLatestBlock(size_t previous_write);
  void IncreaseRead();
  void IncreaseInsert();

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

int RenderDelayBufferImpl::instance_count_ = 0;

RenderDelayBufferImpl::RenderDelayBufferImpl(const EchoCanceller3Config& config,
                                             size_t num_bands)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      api_call_jitter_blocks_(config.delay.api_call_jitter_blocks),
      min_echo_path_delay_blocks_(config.delay.min_echo_path_delay_blocks),
      sub_block_size_(
          static_cast<int>(config.delay.down_sampling_factor > 0
                               ? kBlockSize / config.delay.down_sampling_factor
                               : kBlockSize)),
      blocks_(GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                       config.delay.num_filters),
              num_bands,
              kBlockSize),
      spectra_(blocks_.buffer.size(), kFftLengthBy2Plus1),
      ffts_(blocks_.buffer.size()),
      delay_(min_echo_path_delay_blocks_),
      echo_remover_buffer_(kAdaptiveFilterLength, &blocks_, &spectra_, &ffts_),
      low_rate_(GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                         config.delay.num_filters)),
      render_decimator_(config.delay.down_sampling_factor),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_(),
      render_ds_(sub_block_size_, 0.f) {
  RTC_DCHECK_EQ(blocks_.buffer.size(), ffts_.buffer.size());
  RTC_DCHECK_EQ(spectra_.buffer.size(), ffts_.buffer.size());
  Reset();
  first_reset_occurred_ = false;
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

void RenderDelayBufferImpl::Reset() {
  delay_ = min_echo_path_delay_blocks_;
  const int offset1 = std::max<int>(
      std::min(api_call_jitter_blocks_, min_echo_path_delay_blocks_), 1);
  const int offset2 = static_cast<int>(delay_ + offset1);
  const int offset3 = offset1 * sub_block_size_;
  low_rate_.read = low_rate_.OffsetIndex(low_rate_.write, offset3);
  blocks_.read = blocks_.OffsetIndex(blocks_.write, -offset2);
  spectra_.read = spectra_.OffsetIndex(spectra_.write, offset2);
  ffts_.read = ffts_.OffsetIndex(ffts_.write, offset2);
  render_surplus_ = 0;
  first_reset_occurred_ = true;
}

RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  RTC_DCHECK_EQ(block.size(), blocks_.buffer[0].size());
  RTC_DCHECK_EQ(block[0].size(), blocks_.buffer[0][0].size());
  BufferingEvent event = BufferingEvent::kNone;

  ++render_surplus_;
  if (first_reset_occurred_) {
    ++render_calls_in_a_row_;
    max_api_jitter_ = std::max(max_api_jitter_, render_calls_in_a_row_);
  }

  const size_t previous_write = blocks_.write;
  IncreaseInsert();

  if (low_rate_.read == low_rate_.write || blocks_.read == blocks_.write) {
    // Render overrun due to more render data being inserted than read. Discard
    // the oldest render data.
    event = BufferingEvent::kRenderOverrun;
    IncreaseRead();
  }

  for (size_t k = 0; k < block.size(); ++k) {
    std::copy(block[k].begin(), block[k].end(),
              blocks_.buffer[blocks_.write][k].begin());
  }

  UpdateBuffersWithLatestBlock(previous_write);
  return event;
}

RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl::PrepareCaptureCall() {
  BufferingEvent event = BufferingEvent::kNone;
  render_calls_in_a_row_ = 0;

  if (low_rate_.read == low_rate_.write || blocks_.read == blocks_.write) {
    event = BufferingEvent::kRenderUnderrun;
  } else {
    IncreaseRead();
  }
  --render_surplus_;

  echo_remover_buffer_.UpdateSpectralSum();

  if (render_surplus_ >= static_cast<int>(api_call_jitter_blocks_)) {
    event = BufferingEvent::kApiCallSkew;
    RTC_LOG(LS_WARNING) << "Api call skew detected at " << capture_call_counter_
                        << ".";
  }

  ++capture_call_counter_;
  return event;
}

void RenderDelayBufferImpl::SetDelay(size_t delay) {
  if (delay_ == delay) {
    return;
  }

  const int delta_delay = static_cast<int>(delay_) - static_cast<int>(delay);
  delay_ = delay;
  if (delay_ > MaxDelay()) {
    delay_ = std::min(MaxDelay(), delay);
    RTC_NOTREACHED();
  }

  // Recompute the read indices according to the set delay.
  blocks_.UpdateReadIndex(delta_delay);
  spectra_.UpdateReadIndex(-delta_delay);
  ffts_.UpdateReadIndex(-delta_delay);
}

void RenderDelayBufferImpl::UpdateBuffersWithLatestBlock(
    size_t previous_write) {
  render_decimator_.Decimate(blocks_.buffer[blocks_.write][0], render_ds_);
  std::copy(render_ds_.rbegin(), render_ds_.rend(),
            low_rate_.buffer.begin() + low_rate_.write);

  fft_.PaddedFft(blocks_.buffer[blocks_.write][0],
                 blocks_.buffer[previous_write][0], &ffts_.buffer[ffts_.write]);

  ffts_.buffer[ffts_.write].Spectrum(optimization_,
                                     spectra_.buffer[spectra_.write]);
};

void RenderDelayBufferImpl::IncreaseRead() {
  low_rate_.UpdateReadIndex(-sub_block_size_);
  blocks_.IncReadIndex();
  spectra_.DecReadIndex();
  ffts_.DecReadIndex();
};

void RenderDelayBufferImpl::IncreaseInsert() {
  low_rate_.UpdateWriteIndex(-sub_block_size_);
  blocks_.IncWriteIndex();
  spectra_.DecWriteIndex();
  ffts_.DecWriteIndex();
};

}  // namespace

RenderDelayBuffer* RenderDelayBuffer::Create(const EchoCanceller3Config& config,
                                             size_t num_bands) {
  return new RenderDelayBufferImpl(config, num_bands);
}

}  // namespace webrtc
