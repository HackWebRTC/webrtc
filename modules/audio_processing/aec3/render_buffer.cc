/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_buffer.h"

#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

RenderBuffer::RenderBuffer(size_t num_ffts_for_spectral_sums,
                           MatrixBuffer* block_buffer,
                           VectorBuffer* spectrum_buffer,
                           FftBuffer* fft_buffer)
    : block_buffer_(block_buffer),
      spectrum_buffer_(spectrum_buffer),
      fft_buffer_(fft_buffer),
      spectral_sums_length_(num_ffts_for_spectral_sums) {
  RTC_DCHECK(block_buffer_);
  RTC_DCHECK(spectrum_buffer_);
  RTC_DCHECK(fft_buffer_);
  RTC_DCHECK_GE(fft_buffer_->buffer.size(), spectral_sums_length_);

  Clear();
}

RenderBuffer::~RenderBuffer() = default;

void RenderBuffer::Clear() {
  spectral_sums_.fill(0.f);
}

void RenderBuffer::UpdateSpectralSum() {
  std::fill(spectral_sums_.begin(), spectral_sums_.end(), 0.f);
  size_t position = spectrum_buffer_->read;
  for (size_t j = 0; j < spectral_sums_length_; ++j) {
    for (size_t k = 0; k < spectral_sums_.size(); ++k) {
      spectral_sums_[k] += spectrum_buffer_->buffer[position][k];
    }
    position = spectrum_buffer_->IncIndex(position);
  }
}

}  // namespace webrtc
