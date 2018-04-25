/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/stationarity_estimator.h"

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {

namespace {
constexpr float kMinNoisePower = 10.f;
constexpr int kHangoverBlocks = kNumBlocksPerSecond / 20;
constexpr int kNBlocksAverageInitPhase = 20;
constexpr int kNBlocksInitialPhase = kNumBlocksPerSecond * 2.;
constexpr size_t kLongWindowSize = 13;
}  // namespace

StationarityEstimator::StationarityEstimator()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      idx_lookahead_(kLongWindowSize, 0),
      idx_lookback_(kLongWindowSize, 0) {
  static_assert(StationarityEstimator::CircularBuffer::GetBufferSize() >=
                    (kLongWindowSize + 1),
                "Mismatch between the window size and the buffer size.");
  Reset();
}

StationarityEstimator::~StationarityEstimator() = default;

void StationarityEstimator::Reset() {
  noise_.Reset();
  hangovers_.fill(0);
  stationarity_flags_.fill(false);
}

void StationarityEstimator::Update(rtc::ArrayView<const float> spectrum,
                                   int block_number) {
  if (!buffer_.IsBlockNumberAlreadyUpdated(block_number)) {
    noise_.Update(spectrum);
    WriteInfoFrameInSlot(block_number, spectrum);
  }
}

void StationarityEstimator::UpdateStationarityFlags(size_t current_block_number,
                                                    size_t num_lookahead) {
  RTC_DCHECK_GE(idx_lookahead_.capacity(),
                std::min(num_lookahead + 1, kLongWindowSize));
  idx_lookahead_.resize(std::min(num_lookahead + 1, kLongWindowSize));
  idx_lookback_.resize(0);
  GetSlotsAheadBack(current_block_number);

  for (size_t k = 0; k < stationarity_flags_.size(); ++k) {
    stationarity_flags_[k] = EstimateBandStationarity(k);
  }
  UpdateHangover();
  SmoothStationaryPerFreq();

  data_dumper_->DumpRaw("aec3_stationarity_noise_spectrum", noise_.Spectrum());
}

void StationarityEstimator::WriteInfoFrameInSlot(
    int block_number,
    rtc::ArrayView<const float> spectrum) {
  size_t slot = buffer_.SetBlockNumberInSlot(block_number);
  for (size_t k = 0; k < spectrum.size(); ++k) {
    buffer_.SetElementProperties(spectrum[k], slot, k);
  }
}

bool StationarityEstimator::EstimateBandStationarity(size_t band) const {
  constexpr float kThrStationarity = 10.f;
  float acumPower = 0.f;
  for (auto slot : idx_lookahead_) {
    acumPower += buffer_.GetPowerBand(slot, band);
  }
  for (auto slot : idx_lookback_) {
    acumPower += buffer_.GetPowerBand(slot, band);
  }

  // Generally windowSize is equal to kLongWindowSize
  float windowSize = idx_lookahead_.size() + idx_lookback_.size();
  float noise = windowSize * GetStationarityPowerBand(band);
  RTC_CHECK_LT(0.f, noise);
  bool stationary = acumPower < kThrStationarity * noise;
  data_dumper_->DumpRaw("aec3_stationarity_long_ratio", acumPower / noise);
  return stationary;
}

bool StationarityEstimator::AreAllBandsStationary() {
  for (auto b : stationarity_flags_) {
    if (!b)
      return false;
  }
  return true;
}

void StationarityEstimator::UpdateHangover() {
  bool reduce_hangover = AreAllBandsStationary();
  for (size_t k = 0; k < stationarity_flags_.size(); ++k) {
    if (!stationarity_flags_[k]) {
      hangovers_[k] = kHangoverBlocks;
    } else if (reduce_hangover) {
      hangovers_[k] = std::max(hangovers_[k] - 1, 0);
    }
  }
}

void StationarityEstimator::GetSlotsAheadBack(size_t current_block_number) {
  for (size_t block = 0; block < idx_lookahead_.size(); ++block) {
    idx_lookahead_[block] = buffer_.GetSlotNumber(current_block_number + block);
  }
  size_t num_lookback_blocks;
  if (idx_lookahead_.size() >= kLongWindowSize) {
    RTC_CHECK_EQ(idx_lookahead_.size(), kLongWindowSize);
    num_lookback_blocks = 0;
  } else {
    num_lookback_blocks = kLongWindowSize - idx_lookahead_.size();
  }
  if (current_block_number < num_lookback_blocks) {
    idx_lookback_.resize(0);
  } else {
    for (size_t block = 0; block < num_lookback_blocks; ++block) {
      int block_number = current_block_number - block - 1;
      if (!buffer_.IsBlockNumberAlreadyUpdated(block_number)) {
        break;
      } else {
        RTC_DCHECK_GE(idx_lookback_.capacity(), idx_lookback_.size() + 1);
        idx_lookback_.push_back(buffer_.GetSlotNumber(block_number));
      }
    }
  }
}

void StationarityEstimator::SmoothStationaryPerFreq() {
  std::array<bool, kFftLengthBy2Plus1> all_ahead_stationary_smooth;
  for (size_t k = 1; k < kFftLengthBy2Plus1 - 1; ++k) {
    all_ahead_stationary_smooth[k] = stationarity_flags_[k - 1] &&
                                     stationarity_flags_[k] &&
                                     stationarity_flags_[k + 1];
  }

  all_ahead_stationary_smooth[0] = all_ahead_stationary_smooth[1];
  all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 1] =
      all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 2];

  stationarity_flags_ = all_ahead_stationary_smooth;
}

int StationarityEstimator::instance_count_ = 0;

StationarityEstimator::NoiseSpectrum::NoiseSpectrum() {
  Reset();
}

StationarityEstimator::NoiseSpectrum::~NoiseSpectrum() = default;

void StationarityEstimator::NoiseSpectrum::Reset() {
  block_counter_ = 0;
  noise_spectrum_.fill(kMinNoisePower);
}

void StationarityEstimator::NoiseSpectrum::Update(
    rtc::ArrayView<const float> spectrum) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, spectrum.size());
  ++block_counter_;
  float alpha = GetAlpha();
  for (size_t k = 0; k < spectrum.size(); ++k) {
    if (block_counter_ <= kNBlocksAverageInitPhase) {
      noise_spectrum_[k] += (1.f / kNBlocksAverageInitPhase) * spectrum[k];
    } else {
      noise_spectrum_[k] =
          UpdateBandBySmoothing(spectrum[k], noise_spectrum_[k], alpha);
    }
  }
}

float StationarityEstimator::NoiseSpectrum::GetAlpha() const {
  constexpr float kAlpha = 0.004f;
  constexpr float kAlphaInit = 0.04f;
  constexpr float kTiltAlpha = (kAlphaInit - kAlpha) / kNBlocksInitialPhase;

  if (block_counter_ > (kNBlocksInitialPhase + kNBlocksAverageInitPhase)) {
    return kAlpha;
  } else {
    return kAlphaInit -
           kTiltAlpha * (block_counter_ - kNBlocksAverageInitPhase);
  }
}

float StationarityEstimator::NoiseSpectrum::UpdateBandBySmoothing(
    float power_band,
    float power_band_noise,
    float alpha) const {
  float power_band_noise_updated = power_band_noise;
  if (power_band_noise < power_band) {
    RTC_DCHECK_GT(power_band, 0.f);
    float alpha_inc = alpha * (power_band_noise / power_band);
    if (block_counter_ > kNBlocksInitialPhase) {
      if (10.f * power_band_noise < power_band) {
        alpha_inc *= 0.1f;
      }
    }
    power_band_noise_updated += alpha_inc * (power_band - power_band_noise);
  } else {
    power_band_noise_updated += alpha * (power_band - power_band_noise);
    power_band_noise_updated =
        std::max(power_band_noise_updated, kMinNoisePower);
  }
  return power_band_noise_updated;
}

StationarityEstimator::CircularBuffer::CircularBuffer() {
  for (auto slot : slots_) {
    slot.block_number_ = -1;
  }
}

}  // namespace webrtc
