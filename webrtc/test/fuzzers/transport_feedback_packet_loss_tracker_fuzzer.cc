/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/base/array_view.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

namespace webrtc {

namespace {

class TransportFeedbackGenerator {
 public:
  explicit TransportFeedbackGenerator(rtc::ArrayView<const uint8_t> data)
      : data_(data), ended_(false), data_idx_(0) {}

  void GetNextTransportFeedback(rtcp::TransportFeedback* feedback) {
    uint16_t base_seq_num = 0;
    if (!ReadData<uint16_t>(&base_seq_num)) {
      return;
    }

    const int64_t kBaseTimeUs = 1234;  // Irrelevant to this test.
    feedback->SetBase(base_seq_num, kBaseTimeUs);

    uint16_t num_statuses = 0;
    if (!ReadData<uint16_t>(&num_statuses))
      return;
    num_statuses = std::max<uint16_t>(num_statuses, 1);

    uint16_t seq_num = base_seq_num;
    while (true) {
      uint8_t status_byte = 0;
      if (!ReadData<uint8_t>(&status_byte))
        return;
      // Each status byte contains 8 statuses.
      for (size_t j = 0; j < 8; ++j) {
        if (status_byte & 0x01) {
          feedback->AddReceivedPacket(seq_num, kBaseTimeUs);
        }
        seq_num++;
        if (seq_num >= base_seq_num + num_statuses) {
          feedback->AddReceivedPacket(seq_num, kBaseTimeUs);
          return;
        }
        status_byte >>= 1;
      }
    }
  }

  bool ended() const { return ended_; }

 private:
  template <typename T>
  bool ReadData(T* value) {
    RTC_DCHECK(!ended_);
    if (data_idx_ + sizeof(T) > data_.size()) {
      ended_ = true;
      return false;
    }
    *value = ByteReader<T>::ReadBigEndian(&data_[data_idx_]);
    data_idx_ += sizeof(T);
    return true;
  }

  const rtc::ArrayView<const uint8_t> data_;
  bool ended_;
  size_t data_idx_;
};

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size < sizeof(uint32_t)) {
    return;
  }
  constexpr size_t kSeqNumHalf = 0x8000u;
  const size_t window_size_1 = std::min<size_t>(
      kSeqNumHalf,
      std::max<uint16_t>(1, ByteReader<uint16_t>::ReadBigEndian(data)));
  data += sizeof(uint16_t);
  const size_t window_size_2 = std::min<size_t>(
      kSeqNumHalf,
      std::max<uint16_t>(1, ByteReader<uint16_t>::ReadBigEndian(data)));
  data += sizeof(uint16_t);
  size -= 2 * sizeof(uint16_t);

  TransportFeedbackPacketLossTracker tracker(
      std::min(window_size_1, window_size_2),
      std::max(window_size_1, window_size_2));
  TransportFeedbackGenerator feedback_generator(
      rtc::ArrayView<const uint8_t>(data, size));
  while (!feedback_generator.ended()) {
    rtcp::TransportFeedback feedback;
    feedback_generator.GetNextTransportFeedback(&feedback);
    tracker.OnReceivedTransportFeedback(feedback);
    tracker.Validate();
  }
}

}  // namespace webrtc
