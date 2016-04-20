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
      frame_callback_(frame_callback),
      last_picture_id_(-1),
      last_unwrap_(-1) {
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
      int start_index = index;
      uint16_t start_seq_num = seq_num;

      while (!sequence_buffer_[start_index].frame_begin) {
        sequence_buffer_[start_index].frame_created = true;
        start_index = start_index > 0 ? start_index - 1 : size_ - 1;
        start_seq_num--;
      }
      sequence_buffer_[start_index].frame_created = true;

      std::unique_ptr<RtpFrameObject> frame(
          new RtpFrameObject(this, start_seq_num, seq_num));
      ManageFrame(std::move(frame));
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

void PacketBuffer::ManageFrame(std::unique_ptr<RtpFrameObject> frame) {
  size_t start_index = frame->first_seq_num() % size_;
  VideoCodecType codec_type = data_buffer_[start_index].codec;

  switch (codec_type) {
    case kVideoCodecULPFEC:
    case kVideoCodecRED:
    case kVideoCodecUnknown:
      RTC_NOTREACHED();
      break;
    case kVideoCodecVP8:
      ManageFrameVp8(std::move(frame));
      break;
    case kVideoCodecVP9:
      // TODO(philipel): ManageFrameVp9(std::move(frame));
      break;
    case kVideoCodecH264:
    case kVideoCodecI420:
    case kVideoCodecGeneric:
      ManageFrameGeneric(std::move(frame));
      break;
  }
}

void PacketBuffer::RetryStashedFrames() {
  size_t num_stashed_frames = stashed_frames_.size();

  // Clean up stashed frames if there are too many.
  while (stashed_frames_.size() > kMaxStashedFrames)
    stashed_frames_.pop();

  // Since frames are stashed if there is not enough data to determine their
  // frame references we should at most check |stashed_frames_.size()| in
  // order to not pop and push frames in and endless loop.
  for (size_t i = 0; i < num_stashed_frames && !stashed_frames_.empty(); ++i) {
    std::unique_ptr<RtpFrameObject> frame = std::move(stashed_frames_.front());
    stashed_frames_.pop();
    ManageFrame(std::move(frame));
  }
}

void PacketBuffer::ManageFrameGeneric(
    std::unique_ptr<RtpFrameObject> frame) {
  size_t index = frame->first_seq_num() % size_;
  const VCMPacket& packet = data_buffer_[index];

  if (packet.frameType == kVideoFrameKey)
    last_seq_num_gop_[frame->last_seq_num()] = frame->last_seq_num();

  // We have received a frame but not yet a keyframe, stash this frame.
  if (last_seq_num_gop_.empty()) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  // Clean up info for old keyframes but make sure to keep info
  // for the last keyframe.
  auto clean_to = last_seq_num_gop_.lower_bound(frame->last_seq_num() - 100);
  if (clean_to != last_seq_num_gop_.end())
    last_seq_num_gop_.erase(last_seq_num_gop_.begin(), clean_to);

  // Find the last sequence number of the last frame for the keyframe
  // that this frame indirectly references.
  auto seq_num_it = last_seq_num_gop_.upper_bound(frame->last_seq_num());
  seq_num_it--;

  // Make sure the packet sequence numbers are continuous, otherwise stash
  // this frame.
  if (packet.frameType == kVideoFrameDelta) {
    if (seq_num_it->second !=
        static_cast<uint16_t>(frame->first_seq_num() - 1)) {
      stashed_frames_.emplace(std::move(frame));
      return;
    }
  }

  RTC_DCHECK(AheadOrAt(frame->last_seq_num(), seq_num_it->first));

  // Since keyframes can cause reordering of the frames delivered from
  // FindFrames() we can't simply assign the picture id according to some
  // incrementing counter.
  frame->picture_id = frame->last_seq_num();
  frame->num_references = packet.frameType == kVideoFrameDelta;
  frame->references[0] = seq_num_it->second;
  seq_num_it->second = frame->picture_id;

  last_picture_id_ = frame->picture_id;
  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

void PacketBuffer::ManageFrameVp8(std::unique_ptr<RtpFrameObject> frame) {
  size_t index = frame->first_seq_num() % size_;
  const VCMPacket& packet = data_buffer_[index];
  const RTPVideoHeaderVP8& codec_header =
      packet.codecSpecificHeader.codecHeader.VP8;

  if (codec_header.pictureId == kNoPictureId ||
      codec_header.temporalIdx == kNoTemporalIdx ||
      codec_header.tl0PicIdx == kNoTl0PicIdx) {
    ManageFrameGeneric(std::move(frame));
    return;
  }

  frame->picture_id = codec_header.pictureId % kPicIdLength;

  if (last_unwrap_ == -1)
    last_unwrap_ = codec_header.pictureId;

  if (last_picture_id_ == -1)
    last_picture_id_ = frame->picture_id;

  // Find if there has been a gap in fully received frames and save the picture
  // id of those frames in |not_yet_received_frames_|.
  if (AheadOf<uint8_t, kPicIdLength>(frame->picture_id, last_picture_id_)) {
    last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    while (last_picture_id_ != frame->picture_id) {
      not_yet_received_frames_.insert(last_picture_id_);
      last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    }
  }

  // Clean up info for base layers that are too old.
  uint8_t old_tl0_pic_idx = codec_header.tl0PicIdx - kMaxLayerInfo;
  auto clean_layer_info_to = layer_info_.lower_bound(old_tl0_pic_idx);
  layer_info_.erase(layer_info_.begin(), clean_layer_info_to);

  // Clean up info about not yet received frames that are too old.
  uint16_t old_picture_id = Subtract<kPicIdLength>(frame->picture_id,
                                                   kMaxNotYetReceivedFrames);
  auto clean_frames_to = not_yet_received_frames_.lower_bound(old_picture_id);
  not_yet_received_frames_.erase(not_yet_received_frames_.begin(),
                                 clean_frames_to);

  if (packet.frameType == kVideoFrameKey) {
    frame->num_references = 0;
    layer_info_[codec_header.tl0PicIdx].fill(-1);
    CompletedFrameVp8(std::move(frame));
    return;
  }

  auto layer_info_it = layer_info_.find(codec_header.temporalIdx == 0
                                            ? codec_header.tl0PicIdx - 1
                                            : codec_header.tl0PicIdx);

  // If we don't have the base layer frame yet, stash this frame.
  if (layer_info_it == layer_info_.end()) {
    stashed_frames_.emplace(std::move(frame));
    return;
  }

  // A non keyframe base layer frame has been received, copy the layer info
  // from the previous base layer frame and set a reference to the previous
  // base layer frame.
  if (codec_header.temporalIdx == 0) {
    layer_info_it =
        layer_info_
            .insert(make_pair(codec_header.tl0PicIdx, layer_info_it->second))
            .first;
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];
    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Layer sync frame, this frame only references its base layer frame.
  if (codec_header.layerSync) {
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];

    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Find all references for this frame.
  frame->num_references = 0;
  for (uint8_t layer = 0; layer <= codec_header.temporalIdx; ++layer) {
    RTC_DCHECK_NE(-1, layer_info_it->second[layer]);

    // If we have not yet received a frame between this frame and the referenced
    // frame then we have to wait for that frame to be completed first.
    auto not_received_frame_it =
             not_yet_received_frames_.upper_bound(layer_info_it->second[layer]);
    if (not_received_frame_it != not_yet_received_frames_.end() &&
        AheadOf<uint8_t, kPicIdLength>(frame->picture_id,
                                       *not_received_frame_it)) {
          stashed_frames_.emplace(std::move(frame));
          return;
    }

    ++frame->num_references;
    frame->references[layer] = layer_info_it->second[layer];
  }

  CompletedFrameVp8(std::move(frame));
}

void PacketBuffer::CompletedFrameVp8(std::unique_ptr<RtpFrameObject> frame) {
  size_t index = frame->first_seq_num() % size_;
  const VCMPacket& packet = data_buffer_[index];
  const RTPVideoHeaderVP8& codec_header =
      packet.codecSpecificHeader.codecHeader.VP8;

  uint8_t tl0_pic_idx = codec_header.tl0PicIdx;
  uint8_t temporal_index = codec_header.temporalIdx;
  auto layer_info_it = layer_info_.find(tl0_pic_idx);

  // Update this layer info and newer.
  while (layer_info_it != layer_info_.end()) {
    if (layer_info_it->second[temporal_index] != -1 &&
        AheadOf<uint16_t, kPicIdLength>(layer_info_it->second[temporal_index],
                                        frame->picture_id)) {
      // The frame was not newer, then no subsequent layer info have to be
      // update.
      break;
    }

    layer_info_it->second[codec_header.temporalIdx] = frame->picture_id;
    ++tl0_pic_idx;
    layer_info_it = layer_info_.find(tl0_pic_idx);
  }
  not_yet_received_frames_.erase(frame->picture_id);

  for (size_t r = 0; r < frame->num_references; ++r)
    frame->references[r] = UnwrapPictureId(frame->references[r]);
  frame->picture_id = UnwrapPictureId(frame->picture_id);

  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

uint16_t PacketBuffer::UnwrapPictureId(uint16_t picture_id) {
  if (last_unwrap_ == -1)
    last_unwrap_ = picture_id;

  uint16_t unwrap_truncated = last_unwrap_ % kPicIdLength;
  uint16_t diff = MinDiff<uint8_t, kPicIdLength>(unwrap_truncated, picture_id);

  if (AheadOf<uint8_t, kPicIdLength>(picture_id, unwrap_truncated))
    last_unwrap_ = Add<1 << 16>(last_unwrap_, diff);
  else
    last_unwrap_ = Subtract<1 << 16>(last_unwrap_, diff);

  return last_unwrap_;
}

void PacketBuffer::Flush() {
  rtc::CritScope lock(&crit_);
  for (size_t i = 0; i < size_; ++i)
    sequence_buffer_[i].used = false;

  last_seq_num_gop_.clear();
  while (!stashed_frames_.empty())
    stashed_frames_.pop();
  not_yet_received_frames_.clear();

  first_packet_received_ = false;
}

}  // namespace video_coding
}  // namespace webrtc
