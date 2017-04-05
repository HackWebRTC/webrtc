/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/rtptransport.h"

#include "webrtc/base/checks.h"

namespace webrtc {

void RtpTransport::set_rtcp_packet_transport(
    rtc::PacketTransportInternal* rtcp) {
  RTC_DCHECK(!rtcp_mux_required_);
  rtcp_packet_transport_ = rtcp;
}

}  // namespace webrtc
