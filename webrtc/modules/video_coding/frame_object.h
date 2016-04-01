/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_

#include "webrtc/modules/video_coding/packet.h"

namespace webrtc {
namespace video_coding {

class FrameObject {
 public:
  virtual uint16_t picture_id() const = 0;
  virtual bool GetBitstream(uint8_t* destination) const = 0;
  virtual ~FrameObject() {}
};

class PacketBuffer;

class RtpFrameObject : public FrameObject {
 public:
  RtpFrameObject(PacketBuffer* packet_buffer,
                 uint16_t picture_id,
                 uint16_t first_packet,
                 uint16_t last_packet);
  ~RtpFrameObject();
  uint16_t first_packet() const;
  uint16_t last_packet() const;
  uint16_t picture_id() const override;
  bool GetBitstream(uint8_t* destination) const override;

 private:
  PacketBuffer* packet_buffer_;
  uint16_t picture_id_;
  uint16_t first_packet_;
  uint16_t last_packet_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_FRAME_OBJECT_H_
