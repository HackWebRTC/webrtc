/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"

namespace webrtc {

static const size_t kGenericHeaderLength = 1;

RtpPacketizerGeneric::RtpPacketizerGeneric(FrameType frame_type,
                                           size_t max_payload_len)
    : payload_data_(NULL),
      payload_size_(0),
      max_payload_len_(max_payload_len - kGenericHeaderLength),
      frame_type_(frame_type) {
}

RtpPacketizerGeneric::~RtpPacketizerGeneric() {
}

void RtpPacketizerGeneric::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  payload_data_ = payload_data;
  payload_size_ = payload_size;

  // Fragment packets more evenly by splitting the payload up evenly.
  uint32_t num_packets =
      (payload_size_ + max_payload_len_ - 1) / max_payload_len_;
  payload_length_ = (payload_size_ + num_packets - 1) / num_packets;
  assert(payload_length_ <= max_payload_len_);

  generic_header_ = RtpFormatVideoGeneric::kFirstPacketBit;
}

bool RtpPacketizerGeneric::NextPacket(uint8_t* buffer,
                                      size_t* bytes_to_send,
                                      bool* last_packet) {
  if (payload_size_ < payload_length_) {
    payload_length_ = payload_size_;
  }

  payload_size_ -= payload_length_;
  *bytes_to_send = payload_length_ + kGenericHeaderLength;
  assert(payload_length_ <= max_payload_len_);

  uint8_t* out_ptr = buffer;
  // Put generic header in packet
  if (frame_type_ == kVideoFrameKey) {
    generic_header_ |= RtpFormatVideoGeneric::kKeyFrameBit;
  }
  *out_ptr++ = generic_header_;
  // Remove first-packet bit, following packets are intermediate
  generic_header_ &= ~RtpFormatVideoGeneric::kFirstPacketBit;

  // Put payload in packet
  memcpy(out_ptr, payload_data_, payload_length_);
  payload_data_ += payload_length_;

  *last_packet = payload_size_ <= 0;

  return true;
}

ProtectionType RtpPacketizerGeneric::GetProtectionType() {
  return kProtectedPacket;
}

StorageType RtpPacketizerGeneric::GetStorageType(
    uint32_t retransmission_settings) {
  return kAllowRetransmission;
}

std::string RtpPacketizerGeneric::ToString() {
  return "RtpPacketizerGeneric";
}

RtpDepacketizerGeneric::RtpDepacketizerGeneric(RtpData* const callback)
    : callback_(callback) {
}

bool RtpDepacketizerGeneric::Parse(WebRtcRTPHeader* rtp_header,
                                   const uint8_t* payload_data,
                                   size_t payload_data_length) {
  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  rtp_header->frameType =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0)
          ? kVideoFrameKey
          : kVideoFrameDelta;
  rtp_header->type.Video.isFirstPacket =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;

  if (callback_->OnReceivedPayloadData(
          payload_data, payload_data_length, rtp_header) != 0) {
    return false;
  }
  return true;
}
}  // namespace webrtc
