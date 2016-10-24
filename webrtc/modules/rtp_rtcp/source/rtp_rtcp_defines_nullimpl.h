/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_DEFINES_NULLIMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_DEFINES_NULLIMPL_H_

#include <stddef.h>

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

// Null object version of RtpFeedback.
class NullRtpFeedback : public RtpFeedback {
 public:
  ~NullRtpFeedback() override {}

  int32_t OnInitializeDecoder(int8_t payload_type,
                              const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                              int frequency,
                              size_t channels,
                              uint32_t rate) override;

  void OnIncomingSSRCChanged(uint32_t ssrc) override {}
  void OnIncomingCSRCChanged(uint32_t csrc, bool added) override {}
};

// Null object version of RtpData.
class NullRtpData : public RtpData {
 public:
  ~NullRtpData() override {}

  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                size_t payload_size,
                                const WebRtcRTPHeader* rtp_header) override;

  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_DEFINES_NULLIMPL_H_
