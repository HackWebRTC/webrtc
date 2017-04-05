/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_RTPTRANSPORT_H_
#define WEBRTC_PC_RTPTRANSPORT_H_

namespace rtc {

class PacketTransportInternal;

}  // namespace rtc

namespace webrtc {

class RtpTransport {
 public:
  RtpTransport(const RtpTransport&) = delete;
  RtpTransport& operator=(const RtpTransport&) = delete;

  explicit RtpTransport(bool rtcp_mux_required)
      : rtcp_mux_required_(rtcp_mux_required) {}

  bool rtcp_mux_required() const { return rtcp_mux_required_; }

  rtc::PacketTransportInternal* rtp_packet_transport() const {
    return rtp_packet_transport_;
  }
  void set_rtp_packet_transport(rtc::PacketTransportInternal* rtp) {
    rtp_packet_transport_ = rtp;
  }

  rtc::PacketTransportInternal* rtcp_packet_transport() const {
    return rtcp_packet_transport_;
  }
  void set_rtcp_packet_transport(rtc::PacketTransportInternal* rtcp);

 private:
  // True if RTCP-multiplexing is required. rtcp_packet_transport_ should
  // always be null in this case.
  const bool rtcp_mux_required_;

  // TODO(zstein): Revisit ownership here - transports are currently owned by
  // TransportController
  rtc::PacketTransportInternal* rtp_packet_transport_ = nullptr;
  rtc::PacketTransportInternal* rtcp_packet_transport_ = nullptr;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_RTPTRANSPORT_H_
