/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H265_H_
#define MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H265_H_

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
class VideoRtpDepacketizerH265 : public VideoRtpDepacketizer {
 public:
  ~VideoRtpDepacketizerH265() override = default;

  absl::optional<ParsedRtpPayload> Parse(
      rtc::CopyOnWriteBuffer rtp_payload) override;

private:
  struct ParsedPayload {
    RTPVideoHeader& video_header() { return video; }
    const RTPVideoHeader& video_header() const { return video; }

    RTPVideoHeader video;

    const uint8_t* payload;
    size_t payload_length;
  };

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length);

  bool ParseFuNalu(ParsedPayload* parsed_payload,
                   const uint8_t* payload_data);
  bool ProcessApOrSingleNalu(ParsedPayload* parsed_payload,
                             const uint8_t* payload_data);

  size_t offset_;
  size_t length_;
  std::unique_ptr<rtc::Buffer> modified_buffer_;

};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_VIDEO_RTP_DEPACKETIZER_H265_H_
