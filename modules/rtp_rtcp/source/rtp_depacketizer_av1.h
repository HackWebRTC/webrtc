/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_AV1_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_AV1_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/rtp_rtcp/source/rtp_format.h"

namespace webrtc {

class RtpDepacketizerAv1 : public RtpDepacketizer {
 public:
  RtpDepacketizerAv1() = default;
  RtpDepacketizerAv1(const RtpDepacketizerAv1&) = delete;
  RtpDepacketizerAv1& operator=(const RtpDepacketizerAv1&) = delete;
  ~RtpDepacketizerAv1() override = default;

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_AV1_H_
