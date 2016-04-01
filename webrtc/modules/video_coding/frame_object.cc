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

RtpFrameObject::RtpFrameObject(PacketBuffer* packet_buffer,
                               uint16_t picture_id,
                               uint16_t first_packet,
                               uint16_t last_packet)
    : packet_buffer_(packet_buffer),
      first_packet_(first_packet),
      last_packet_(last_packet) {}

RtpFrameObject::~RtpFrameObject() {
  packet_buffer_->ReturnFrame(this);
}

uint16_t RtpFrameObject::first_packet() const {
  return first_packet_;
}

uint16_t RtpFrameObject::last_packet() const {
  return last_packet_;
}

uint16_t RtpFrameObject::picture_id() const {
  return picture_id_;
}

bool RtpFrameObject::GetBitstream(uint8_t* destination) const {
  return packet_buffer_->GetBitstream(*this, destination);
}

}  // namespace video_coding
}  // namespace webrtc
