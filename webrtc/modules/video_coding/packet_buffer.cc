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
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"

namespace webrtc {
namespace video_coding {

PacketBuffer::PacketBuffer(size_t start_buffer_size,
                           size_t max_buffer_size,
                           OnCompleteFrameCallback* frame_callback)
    : size_(start_buffer_size),
      max_size_(max_buffer_size),
      last_seq_num_(0),
      first_seq_num_(0),
      initialized_(false),
      data_buffer_(start_buffer_size),
      sequence_buffer_(start_buffer_size),
      frame_callback_(frame_callback) {
  RTC_DCHECK_LE(start_buffer_size, max_buffer_size);
  // Buffer size must always be a power of 2.
  RTC_DCHECK((start_buffer_size & (start_buffer_size - 1)) == 0);
  RTC_DCHECK((max_buffer_size & (max_buffer_size - 1)) == 0);
}

bool PacketBuffer::InsertPacket(const VCMPacket& packet) {
  rtc::CritScope lock(&crit_);
  uint16_t seq_num = packet.seqNum;
  int index = seq_num % size_;

  if (!initialized_) {
    first_seq_num_ = seq_num - 1;
    last_seq_num_ = seq_num;
    initialized_ = true;
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
  sequence_buffer_[index].used = true;
  data_buffer_[index] = packet;

  FindCompleteFrames(seq_num);
  return true;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  int index = first_seq_num_ % size_;
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
      int index = sequence_buffer_[i].seq_num % new_size;
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
  int index = seq_num % size_;
  int prev_index = index > 0 ? index - 1 : size_ - 1;
  if (!sequence_buffer_[index].used)
    return false;
  if (sequence_buffer_[index].frame_begin)
    return true;
  if (!sequence_buffer_[prev_index].used)
    return false;
  if (sequence_buffer_[prev_index].continuous)
    return true;

  return false;
}

void PacketBuffer::FindCompleteFrames(uint16_t seq_num) {
  int index = seq_num % size_;
  while (IsContinuous(seq_num)) {
    sequence_buffer_[index].continuous = true;

    // If the frame is complete, find the first packet of the frame and
    // create a FrameObject.
    if (sequence_buffer_[index].frame_end) {
      int rindex = index;
      uint16_t start_seq_num = seq_num;
      while (!sequence_buffer_[rindex].frame_begin) {
        rindex = rindex > 0 ? rindex - 1 : size_ - 1;
        start_seq_num--;
      }

      std::unique_ptr<FrameObject> frame(
          new RtpFrameObject(this, 1, start_seq_num, seq_num));
      frame_callback_->OnCompleteFrame(std::move(frame));
    }

    index = (index + 1) % size_;
    ++seq_num;
  }
}

void PacketBuffer::ReturnFrame(RtpFrameObject* frame) {
  rtc::CritScope lock(&crit_);
  int index = frame->first_packet() % size_;
  int end = (frame->last_packet() + 1) % size_;
  uint16_t seq_num = frame->first_packet();
  while (index != end) {
    if (sequence_buffer_[index].seq_num == seq_num) {
      sequence_buffer_[index].used = false;
      sequence_buffer_[index].continuous = false;
    }
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

  int index = frame.first_packet() % size_;
  int end = (frame.last_packet() + 1) % size_;
  uint16_t seq_num = frame.first_packet();
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

void PacketBuffer::Flush() {
  rtc::CritScope lock(&crit_);
  for (size_t i = 0; i < size_; ++i) {
    sequence_buffer_[i].used = false;
    sequence_buffer_[i].continuous = false;
  }
}

}  // namespace video_coding
}  // namespace webrtc
