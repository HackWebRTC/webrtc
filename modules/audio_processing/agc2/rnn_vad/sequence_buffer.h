/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SEQUENCE_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SEQUENCE_BUFFER_H_

#include <array>
#include <cstring>
#include <type_traits>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

// Linear buffer implementation to (i) push fixed size chunks of sequential data
// and (ii) view contiguous parts of the buffer. The buffer and the pushed
// chunks have size S and N respectively. For instance, when S = 2N the first
// half of the sequence buffer is replaced with its second half, and the new N
// values are written at the end of the buffer.
template <typename T, size_t S, size_t N>
class SequenceBuffer {
  static_assert(S >= N,
                "The new chunk size is larger than the sequence buffer size.");
  static_assert(std::is_arithmetic<T>::value,
                "Integral or floating point required.");

 public:
  SequenceBuffer() { buffer_.fill(0); }
  SequenceBuffer(const SequenceBuffer&) = delete;
  SequenceBuffer& operator=(const SequenceBuffer&) = delete;
  ~SequenceBuffer() = default;
  size_t size() const { return S; }
  size_t chunks_size() const { return N; }
  // Sets the sequence buffer values to zero.
  void Reset() { buffer_.fill(0); }
  // Returns a view on the whole buffer.
  rtc::ArrayView<const T, S> GetBufferView() const {
    return {buffer_.data(), S};
  }
  // Returns a view on part of the buffer; the first element starts at the given
  // offset and the last one is the last one in the buffer.
  rtc::ArrayView<const T> GetBufferView(int offset) const {
    RTC_DCHECK_LE(0, offset);
    RTC_DCHECK_LT(offset, S);
    return {buffer_.data() + offset, S - offset};
  }
  // Returns a view on part of the buffer; the first element starts at the given
  // offset and the size of the view is |size|.
  rtc::ArrayView<const T> GetBufferView(int offset, size_t size) const {
    RTC_DCHECK_LE(0, offset);
    RTC_DCHECK_LT(offset, S);
    RTC_DCHECK_LT(0, size);
    RTC_DCHECK_LE(size, S - offset);
    return {buffer_.data() + offset, size};
  }
  // Shifts left the buffer by N items and add new N items at the end.
  void Push(rtc::ArrayView<const T, N> new_values) {
    // Make space for the new values.
    if (S > N)
      std::memmove(buffer_.data(), buffer_.data() + N, (S - N) * sizeof(T));
    // Copy the new values at the end of the buffer.
    std::memcpy(buffer_.data() + S - N, new_values.data(), N * sizeof(T));
  }

 private:
  // TODO(bugs.webrtc.org/9076): Switch to std::vector to decrease stack size.
  std::array<T, S> buffer_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SEQUENCE_BUFFER_H_
