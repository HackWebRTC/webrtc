/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

FrameObject::FrameObject()
    : picture_id(0),
      spatial_layer(0),
      timestamp(0),
      num_references(0),
      inter_layer_predicted(false) {}

RtpFrameObject::RtpFrameObject(PacketBuffer* packet_buffer,
                               uint16_t first_seq_num,
                               uint16_t last_seq_num,
                               size_t frame_size,
                               int times_nacked)
    : packet_buffer_(packet_buffer),
      first_seq_num_(first_seq_num),
      last_seq_num_(last_seq_num),
      times_nacked_(times_nacked) {
  size = frame_size;
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num);
  if (packet) {
    frame_type_ = packet->frameType;
    codec_type_ = packet->codec;
  }
}

RtpFrameObject::~RtpFrameObject() {
  packet_buffer_->ReturnFrame(this);
}

uint16_t RtpFrameObject::first_seq_num() const {
  return first_seq_num_;
}

uint16_t RtpFrameObject::last_seq_num() const {
  return last_seq_num_;
}

int RtpFrameObject::times_nacked() const {
  return times_nacked_;
}

FrameType RtpFrameObject::frame_type() const {
  return frame_type_;
}

VideoCodecType RtpFrameObject::codec_type() const {
  return codec_type_;
}

bool RtpFrameObject::GetBitstream(uint8_t* destination) const {
  return packet_buffer_->GetBitstream(*this, destination);
}

RTPVideoTypeHeader* RtpFrameObject::GetCodecHeader() const {
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num_);
  if (!packet)
    return nullptr;
  return &packet->video_header.codecHeader;
}

}  // namespace video_coding
}  // namespace webrtc
