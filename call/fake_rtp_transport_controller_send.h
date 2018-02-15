/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include "call/rtp_transport_controller_send_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"

namespace webrtc {

class FakeRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
 public:
  explicit FakeRtpTransportControllerSend(
      PacketRouter* packet_router,
      PacedSender* paced_sender,
      SendSideCongestionController* send_side_cc)
      : packet_router_(packet_router),
        paced_sender_(paced_sender),
        send_side_cc_(send_side_cc) {
    RTC_DCHECK(send_side_cc);
  }

  PacketRouter* packet_router() override { return packet_router_; }

  TransportFeedbackObserver* transport_feedback_observer() override {
    return send_side_cc_;
  }

  PacedSender* pacer() override { return paced_sender_; }

  RtpPacketSender* packet_sender() override { return paced_sender_; }

  const RtpKeepAliveConfig& keepalive_config() const override {
    return keepalive_;
  }

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps) override {}

  void set_keepalive_config(const RtpKeepAliveConfig& keepalive_config) {
    keepalive_ = keepalive_config;
  }

  Module* GetModule() override { return send_side_cc_; }
  CallStatsObserver* GetCallStatsObserver() override { return send_side_cc_; }
  void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override {
    send_side_cc_->RegisterPacketFeedbackObserver(observer);
  }
  void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override {
    send_side_cc_->DeRegisterPacketFeedbackObserver(observer);
  }
  void RegisterNetworkObserver(NetworkChangedObserver* observer) override {
    send_side_cc_->RegisterNetworkObserver(observer);
  }
  void DeRegisterNetworkObserver(NetworkChangedObserver* observer) override {
    send_side_cc_->RegisterNetworkObserver(observer);
  }
  void SetBweBitrates(int min_bitrate_bps,
                      int start_bitrate_bps,
                      int max_bitrate_bps) override {
    send_side_cc_->SetBweBitrates(min_bitrate_bps, start_bitrate_bps,
                                  max_bitrate_bps);
  }
  void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                             int start_bitrate_bps,
                             int min_bitrate_bps,
                             int max_bitrate_bps) override {
    send_side_cc_->OnNetworkRouteChanged(network_route, start_bitrate_bps,
                                         min_bitrate_bps, max_bitrate_bps);
  }
  void OnNetworkAvailability(bool network_available) override {
    send_side_cc_->SignalNetworkState(network_available ? kNetworkUp
                                                        : kNetworkDown);
  }
  void SetTransportOverhead(
      size_t transport_overhead_bytes_per_packet) override {
    send_side_cc_->SetTransportOverhead(transport_overhead_bytes_per_packet);
  }
  RtcpBandwidthObserver* GetBandwidthObserver() override {
    return send_side_cc_->GetBandwidthObserver();
  }
  bool AvailableBandwidth(uint32_t* bandwidth) const override {
    return send_side_cc_->AvailableBandwidth(bandwidth);
  }
  int64_t GetPacerQueuingDelayMs() const override {
    return send_side_cc_->GetPacerQueuingDelayMs();
  }
  int64_t GetFirstPacketTimeMs() const override {
    return send_side_cc_->GetFirstPacketTimeMs();
  }
  RateLimiter* GetRetransmissionRateLimiter() override {
    return send_side_cc_->GetRetransmissionRateLimiter();
  }
  void EnablePeriodicAlrProbing(bool enable) override {
    send_side_cc_->EnablePeriodicAlrProbing(enable);
  }
  void OnSentPacket(const rtc::SentPacket& sent_packet) override {
    send_side_cc_->OnSentPacket(sent_packet);
  }

 private:
  PacketRouter* packet_router_;
  PacedSender* paced_sender_;
  SendSideCongestionController* send_side_cc_;
  RtpKeepAliveConfig keepalive_;
};

}  // namespace webrtc

#endif  // CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
