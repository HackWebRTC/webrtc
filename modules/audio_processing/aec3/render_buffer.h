/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_

#include <array>
#include <memory>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "modules/audio_processing/aec3/vector_buffer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Provides a buffer of the render data for the echo remover.
class RenderBuffer {
 public:
  RenderBuffer(size_t num_ffts_for_spectral_sums,
               MatrixBuffer* block_buffer,
               VectorBuffer* spectrum_buffer,
               FftBuffer* fft_buffer);
  ~RenderBuffer();

  // Clears the buffer.
  void Clear();

  // Insert a block into the buffer.
  void UpdateSpectralSum();

  // Gets the last inserted block.
  const std::vector<std::vector<float>>& MostRecentBlock() const {
    return block_buffer_->buffer[block_buffer_->read];
  }

  // Get the spectrum from one of the FFTs in the buffer.
  rtc::ArrayView<const float> Spectrum(size_t buffer_offset_ffts) const {
    size_t position = spectrum_buffer_->OffsetIndex(spectrum_buffer_->read,
                                                    buffer_offset_ffts);
    return spectrum_buffer_->buffer[position];
  }

  // Returns the sum of the spectrums for a certain number of FFTs.
  rtc::ArrayView<const float> SpectralSum(size_t num_ffts) const {
    RTC_DCHECK_EQ(spectral_sums_length_, num_ffts);
    return spectral_sums_;
  }

  // Returns the circular buffer.
  rtc::ArrayView<const FftData> Buffer() const { return fft_buffer_->buffer; }

  // Returns the current position in the circular buffer.
  size_t Position() const { return fft_buffer_->read; }

 private:
  const MatrixBuffer* const block_buffer_;
  const VectorBuffer* const spectrum_buffer_;
  const FftBuffer* const fft_buffer_;
  const size_t spectral_sums_length_;
  std::array<float, kFftLengthBy2Plus1> spectral_sums_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderBuffer);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_
