/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
#define MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "modules/rtp_rtcp/source/rtp_format.h"
#include "rtc_base/buffer.h"

namespace webrtc {
class RtpDepacketizerH264 : public RtpDepacketizer {
 public:
  RtpDepacketizerH264();
  ~RtpDepacketizerH264() override;

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;

 private:
  bool ParseFuaNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                    const uint8_t* payload_data);
  bool ProcessStapAOrSingleNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                                const uint8_t* payload_data);

  size_t offset_;
  size_t length_;
  std::unique_ptr<rtc::Buffer> modified_buffer_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H264_H_
