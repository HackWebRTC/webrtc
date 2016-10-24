/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_defines_nullimpl.h"

namespace webrtc {

int32_t NullRtpFeedback::OnInitializeDecoder(
    int8_t payload_type,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    int frequency,
    size_t channels,
    uint32_t rate) {
  return 0;
}

int32_t NullRtpData::OnReceivedPayloadData(const uint8_t* payload_data,
                                           size_t payload_size,
                                           const WebRtcRTPHeader* rtp_header) {
  return 0;
}

bool NullRtpData::OnRecoveredPacket(const uint8_t* packet,
                                    size_t packet_length) {
  return true;
}

}  // namespace webrtc
