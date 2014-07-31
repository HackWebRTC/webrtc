/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class RtpPacketizer {
 public:
  static RtpPacketizer* Create(RtpVideoCodecTypes type, size_t max_payload_len);

  virtual ~RtpPacketizer() {}

  virtual void SetPayloadData(const uint8_t* payload_data,
                              size_t payload_size,
                              const RTPFragmentationHeader* fragmentation) = 0;

  // Get the next payload with payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  virtual bool NextPacket(uint8_t* buffer,
                          size_t* bytes_to_send,
                          bool* last_packet) = 0;
};

class RtpDepacketizer {
 public:
  static RtpDepacketizer* Create(RtpVideoCodecTypes type,
                                 RtpData* const callback);

  virtual ~RtpDepacketizer() {}

  virtual bool Parse(WebRtcRTPHeader* rtp_header,
                     const uint8_t* payload_data,
                     size_t payload_data_length) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
