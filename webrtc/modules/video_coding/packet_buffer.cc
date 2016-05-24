/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/packet_buffer.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/frame_object.h"

namespace webrtc {
namespace video_coding {

PacketBuffer::PacketBuffer(size_t start_buffer_size,
                           size_t max_buffer_size,
                           OnCompleteFrameCallback* frame_callback)
    : size_(start_buffer_size),
      max_size_(max_buffer_size),
      first_seq_num_(0),
      last_seq_num_(0),
      first_packet_received_(false),
      data_buffer_(start_buffer_size),
      sequence_buffer_(start_buffer_size),
      reference_finder_(frame_callback) {
  RTC_DCHECK_LE(start_buffer_size, max_buffer_size);
  // Buffer size must always be a power of 2.
  RTC_DCHECK((start_buffer_size & (start_buffer_size - 1)) == 0);
  RTC_DCHECK((max_buffer_size & (max_buffer_size - 1)) == 0);
}

bool PacketBuffer::InsertPacket(const VCMPacket& packet) {
  rtc::CritScope lock(&crit_);
  uint16_t seq_num = packet.seqNum;
  size_t index = seq_num % size_;

  if (!first_packet_received_) {
    first_seq_num_ = seq_num - 1;
    last_seq_num_ = seq_num;
    first_packet_received_ = true;
  }

  if (sequence_buffer_[index].used) {
    // Duplicate packet, do nothing.
    if (data_buffer_[index].seqNum == packet.seqNum)
      return true;

    // The packet buffer is full, try to expand the buffer.
    while (ExpandBufferSize() && sequence_buffer_[seq_num % size_].used) {
    }
    index = seq_num % size_;

    // Packet buffer is still full.
    if (sequence_buffer_[index].used)
      return false;
  }

  if (AheadOf(seq_num, last_seq_num_))
    last_seq_num_ = seq_num;

  sequence_buffer_[index].frame_begin = packet.isFirstPacket;
  sequence_buffer_[index].frame_end = packet.markerBit;
  sequence_buffer_[index].seq_num = packet.seqNum;
  sequence_buffer_[index].continuous = false;
  sequence_buffer_[index].frame_created = false;
  sequence_buffer_[index].used = true;
  data_buffer_[index] = packet;

  FindFrames(seq_num);
  return true;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  size_t index = first_seq_num_ % size_;
  while (AheadOf<uint16_t>(seq_num, first_seq_num_ + 1)) {
    index = (index + 1) % size_;
    first_seq_num_ = Add<1 << 16>(first_seq_num_, 1);
    sequence_buffer_[index].used = false;
  }
}

bool PacketBuffer::ExpandBufferSize() {
  if (size_ == max_size_)
    return false;

  size_t new_size = std::min(max_size_, 2 * size_);
  std::vector<VCMPacket> new_data_buffer(new_size);
  std::vector<ContinuityInfo> new_sequence_buffer(new_size);
  for (size_t i = 0; i < size_; ++i) {
    if (sequence_buffer_[i].used) {
      size_t index = sequence_buffer_[i].seq_num % new_size;
      new_sequence_buffer[index] = sequence_buffer_[i];
      new_data_buffer[index] = data_buffer_[i];
    }
  }
  size_ = new_size;
  sequence_buffer_ = std::move(new_sequence_buffer);
  data_buffer_ = std::move(new_data_buffer);
  return true;
}

bool PacketBuffer::IsContinuous(uint16_t seq_num) const {
  size_t index = seq_num % size_;
  int prev_index = index > 0 ? index - 1 : size_ - 1;

  if (!sequence_buffer_[index].used)
    return false;
  if (sequence_buffer_[index].frame_created)
    return false;
  if (sequence_buffer_[index].frame_begin)
    return true;
  if (!sequence_buffer_[prev_index].used)
    return false;
  if (sequence_buffer_[prev_index].seq_num !=
      static_cast<uint16_t>(seq_num - 1))
    return false;
  if (sequence_buffer_[prev_index].continuous)
    return true;

  return false;
}

void PacketBuffer::FindFrames(uint16_t seq_num) {
  size_t index = seq_num % size_;
  while (IsContinuous(seq_num)) {
    sequence_buffer_[index].continuous = true;

    // If all packets of the frame is continuous, find the first packet of the
    // frame and create an RtpFrameObject.
    if (sequence_buffer_[index].frame_end) {
      size_t frame_size = 0;
      int max_nack_count = -1;
      uint16_t start_seq_num = seq_num;

      // Find the start index by searching backward until the packet with
      // the |frame_begin| flag is set.
      int start_index = index;
      while (true) {
        frame_size += data_buffer_[start_index].sizeBytes;
        max_nack_count = std::max(
            max_nack_count, data_buffer_[start_index].timesNacked);
        sequence_buffer_[start_index].frame_created = true;

        if (sequence_buffer_[start_index].frame_begin)
          break;

        start_index = start_index > 0 ? start_index - 1 : size_ - 1;
        start_seq_num--;
      }

      std::unique_ptr<RtpFrameObject> frame(new RtpFrameObject(
          this, start_seq_num, seq_num, frame_size, max_nack_count));
      reference_finder_.ManageFrame(std::move(frame));
    }

    index = (index + 1) % size_;
    ++seq_num;
  }
}

void PacketBuffer::ReturnFrame(RtpFrameObject* frame) {
  rtc::CritScope lock(&crit_);
  size_t index = frame->first_seq_num() % size_;
  size_t end = (frame->last_seq_num() + 1) % size_;
  uint16_t seq_num = frame->first_seq_num();
  while (index != end) {
    if (sequence_buffer_[index].seq_num == seq_num)
      sequence_buffer_[index].used = false;

    index = (index + 1) % size_;
    ++seq_num;
  }

  index = first_seq_num_ % size_;
  while (AheadOf<uint16_t>(last_seq_num_, first_seq_num_) &&
         !sequence_buffer_[index].used) {
    ++first_seq_num_;
    index = (index + 1) % size_;
  }
}

bool PacketBuffer::GetBitstream(const RtpFrameObject& frame,
                                uint8_t* destination) {
  rtc::CritScope lock(&crit_);

  size_t index = frame.first_seq_num() % size_;
  size_t end = (frame.last_seq_num() + 1) % size_;
  uint16_t seq_num = frame.first_seq_num();
  while (index != end) {
    if (!sequence_buffer_[index].used ||
        sequence_buffer_[index].seq_num != seq_num) {
      return false;
    }

    const uint8_t* source = data_buffer_[index].dataPtr;
    size_t length = data_buffer_[index].sizeBytes;
    memcpy(destination, source, length);
    destination += length;
    index = (index + 1) % size_;
    ++seq_num;
  }
  return true;
}

VCMPacket* PacketBuffer::GetPacket(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  size_t index = seq_num % size_;
  if (!sequence_buffer_[index].used ||
      seq_num != sequence_buffer_[index].seq_num) {
    return nullptr;
  }
  return &data_buffer_[index];
}

void PacketBuffer::Clear() {
  rtc::CritScope lock(&crit_);
  for (size_t i = 0; i < size_; ++i)
    sequence_buffer_[i].used = false;

  first_packet_received_ = false;
}

}  // namespace video_coding
}  // namespace webrtc
