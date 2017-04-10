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
#include "webrtc/p2p/base/packettransportinterface.h"

namespace webrtc {

void RtpTransport::set_rtp_packet_transport(rtc::PacketTransportInternal* rtp) {
  rtp_packet_transport_ = rtp;
}

void RtpTransport::set_rtcp_packet_transport(
    rtc::PacketTransportInternal* rtcp) {
  RTC_DCHECK(!rtcp_mux_required_);
  rtcp_packet_transport_ = rtcp;
}

PacketTransportInterface* RtpTransport::GetRtpPacketTransport() const {
  return rtp_packet_transport_;
}

PacketTransportInterface* RtpTransport::GetRtcpPacketTransport() const {
  return rtcp_packet_transport_;
}

RTCError RtpTransport::SetRtcpParameters(const RtcpParameters& parameters) {
  if (rtcp_parameters_.mux && !parameters.mux) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "Disabling RTCP muxing is not allowed.");
  }

  RtcpParameters new_parameters = parameters;

  if (new_parameters.cname.empty()) {
    new_parameters.cname = rtcp_parameters_.cname;
  }

  rtcp_parameters_ = new_parameters;
  return RTCError::OK();
}

RtcpParameters RtpTransport::GetRtcpParameters() const {
  return rtcp_parameters_;
}

RtpTransportAdapter* RtpTransport::GetInternal() {
  return nullptr;
}

}  // namespace webrtc
