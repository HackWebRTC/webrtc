/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_SRTPTRANSPORT_H_
#define WEBRTC_PC_SRTPTRANSPORT_H_

#include <memory>
#include <string>
#include <utility>

#include "webrtc/pc/rtptransportinternal.h"
#include "webrtc/pc/srtpfilter.h"
#include "webrtc/rtc_base/checks.h"

namespace webrtc {

// This class will eventually be a wrapper around RtpTransportInternal
// that protects and unprotects sent and received RTP packets. This
// functionality is currently implemented by SrtpFilter and BaseChannel, but
// will be moved here in the future.
class SrtpTransport : public RtpTransportInternal {
 public:
  SrtpTransport(bool rtcp_mux_enabled, const std::string& content_name);

  // TODO(zstein): Consider taking an RtpTransport instead of an
  // RtpTransportInternal.
  SrtpTransport(std::unique_ptr<RtpTransportInternal> transport,
                const std::string& content_name);

  void SetRtcpMuxEnabled(bool enable) override {
    rtp_transport_->SetRtcpMuxEnabled(enable);
  }

  rtc::PacketTransportInternal* rtp_packet_transport() const override {
    return rtp_transport_->rtp_packet_transport();
  }

  void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp) override {
    rtp_transport_->SetRtpPacketTransport(rtp);
  }

  PacketTransportInterface* GetRtpPacketTransport() const override {
    return rtp_transport_->GetRtpPacketTransport();
  }

  rtc::PacketTransportInternal* rtcp_packet_transport() const override {
    return rtp_transport_->rtcp_packet_transport();
  }
  void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp) override {
    rtp_transport_->SetRtcpPacketTransport(rtcp);
  }

  PacketTransportInterface* GetRtcpPacketTransport() const override {
    return rtp_transport_->GetRtcpPacketTransport();
  }

  bool IsWritable(bool rtcp) const override {
    return rtp_transport_->IsWritable(rtcp);
  }

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags) override;

  bool HandlesPayloadType(int payload_type) const override {
    return rtp_transport_->HandlesPayloadType(payload_type);
  }

  void AddHandledPayloadType(int payload_type) override {
    rtp_transport_->AddHandledPayloadType(payload_type);
  }

  RTCError SetParameters(const RtpTransportParameters& parameters) override {
    return rtp_transport_->SetParameters(parameters);
  }

  RtpTransportParameters GetParameters() const override {
    return rtp_transport_->GetParameters();
  }

  // TODO(zstein): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override { return nullptr; }

 private:
  void ConnectToRtpTransport();

  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time);

  void OnReadyToSend(bool ready) { SignalReadyToSend(ready); }

  const std::string content_name_;

  std::unique_ptr<RtpTransportInternal> rtp_transport_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_SRTPTRANSPORT_H_
