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

template <typename T>
T FuzzInput(const uint8_t** data, size_t* size) {
  RTC_CHECK_GE(*size, sizeof(T));
  T rc = ByteReader<T>::ReadBigEndian(*data);
  *data += sizeof(T);
  *size -= sizeof(T);
  return rc;
}

size_t FuzzInRange(const uint8_t** data,
                   size_t* size,
                   size_t lower,
                   size_t upper) {
  // Achieve a close-to-uniform distribution.
  RTC_CHECK_LE(lower, upper);
  RTC_CHECK_LT(upper - lower, 1 << (8 * sizeof(uint16_t)));
  const size_t range = upper - lower;
  const uint16_t fuzzed = FuzzInput<uint16_t>(data, size);
  const size_t offset = (static_cast<float>(fuzzed) / 0x10000) * (range + 1);
  RTC_CHECK_LE(offset, range);  // (fuzzed <= 0xffff) -> (offset < range + 1)
  return lower + offset;
}

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
    RTC_CHECK(!ended_);
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
  if (size < 3 * sizeof(uint16_t)) {
    return;
  }
  constexpr size_t kSeqNumHalf = 0x8000u;

  // 0x8000 >= max_window_size >= plr_min_num_packets > rplr_min_num_pairs >= 1
  // (The distribution isn't uniform, but it's enough; more would be overkill.)
  const size_t max_window_size = FuzzInRange(&data, &size, 2, kSeqNumHalf);
  const size_t plr_min_num_packets =
      FuzzInRange(&data, &size, 2, max_window_size);
  const size_t rplr_min_num_pairs =
      FuzzInRange(&data, &size, 1, plr_min_num_packets - 1);

  TransportFeedbackPacketLossTracker tracker(
      max_window_size, plr_min_num_packets, rplr_min_num_pairs);

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
