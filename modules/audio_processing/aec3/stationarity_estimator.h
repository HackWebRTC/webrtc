/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_

#include <array>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class ApmDataDumper;

class StationarityEstimator {
 public:
  StationarityEstimator();
  ~StationarityEstimator();

  // Reset the stationarity estimator.
  void Reset();

  // Update the stationarity estimator.
  void Update(rtc::ArrayView<const float> spectrum, int block_number);

  // Update just the noise estimator. Usefull until the delay is known
  void UpdateNoiseEstimator(rtc::ArrayView<const float> spectrum);

  // Update the flag indicating whether this current frame is stationary. For
  // getting a more robust estimation, it looks at future and/or past frames.
  void UpdateStationarityFlags(size_t current_block_number,
                               size_t num_lookahead);

  // Returns true if the current band is stationary.
  bool IsBandStationary(size_t band) const {
    return stationarity_flags_[band] && (hangovers_[band] == 0);
  }

  static constexpr size_t GetMaxNumLookAhead() {
    return CircularBuffer::GetBufferSize() - 2;
  }

 private:
  // Returns the power of the stationary noise spectrum at a band.
  float GetStationarityPowerBand(size_t k) const { return noise_.Power(k); }

  // Write into the slot the information about the current frame that
  // is needed for the stationarity detection.
  void WriteInfoFrameInSlot(int block_number,
                            rtc::ArrayView<const float> spectrum);

  // Get an estimation of the stationarity for the current band by looking
  // at the past/present/future available data.
  bool EstimateBandStationarity(size_t band) const;

  // True if all bands at the current point are stationary.
  bool AreAllBandsStationary();

  // Update the hangover depending on the stationary status of the current
  // frame.
  void UpdateHangover();

  // Get the slots that contain past/present and future data.
  void GetSlotsAheadBack(size_t current_block_number);

  // Smooth the stationarity detection by looking at neighbouring frequency
  // bands.
  void SmoothStationaryPerFreq();

  class NoiseSpectrum {
   public:
    NoiseSpectrum();
    ~NoiseSpectrum();

    // Reset the noise power spectrum estimate state.
    void Reset();

    // Update the noise power spectrum with a new frame.
    void Update(rtc::ArrayView<const float> spectrum);

    // Get the noise estimation power spectrum.
    rtc::ArrayView<const float> Spectrum() const { return noise_spectrum_; }

    // Get the noise power spectrum at a certain band.
    float Power(size_t band) const {
      RTC_DCHECK_LT(band, noise_spectrum_.size());
      return noise_spectrum_[band];
    }

   private:
    // Get the update coefficient to be used for the current frame.
    float GetAlpha() const;

    // Update the noise power spectrum at a certain band with a new frame.
    float UpdateBandBySmoothing(float power_band,
                                float power_band_noise,
                                float alpha) const;
    std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
    size_t block_counter_;
  };

  // The class circular buffer stores the data needed to take a decission
  // on whether the current frame is stationary by looking at data from the
  // future, present and/or past. This buffer stores that data that is
  // represented by the struct Element a bit bellow.
  class CircularBuffer {
   public:
    static constexpr int kCircularBufferSize = 16;
    struct Element {
      int block_number_;
      std::array<float, kFftLengthBy2Plus1> power_spectrum_;
    };
    CircularBuffer();

    static constexpr int GetBufferSize() { return kCircularBufferSize; }

    bool IsBlockNumberAlreadyUpdated(int block_number) const {
      size_t slot_number = GetSlotNumber(block_number);
      return slots_[slot_number].block_number_ == block_number;
    }

    size_t GetSlotNumber(int block_number) const {
      return block_number & (kCircularBufferSize - 1);
    }
    size_t SetBlockNumberInSlot(int block_number) {
      size_t slot = GetSlotNumber(block_number);
      slots_[slot].block_number_ = block_number;
      return slot;
    }
    void SetElementProperties(float band_power, int slot, int band) {
      slots_[slot].power_spectrum_[band] = band_power;
    }
    float GetPowerBand(size_t slot, size_t band) const {
      return slots_[slot].power_spectrum_[band];
    }

   private:
    std::array<Element, kCircularBufferSize> slots_;
  };
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  NoiseSpectrum noise_;
  std::vector<size_t> idx_lookahead_;
  std::vector<size_t> idx_lookback_;
  std::array<int, kFftLengthBy2Plus1> hangovers_;
  std::array<bool, kFftLengthBy2Plus1> stationarity_flags_;
  CircularBuffer buffer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
