/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_transport_controller_send.h"

namespace webrtc {

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log)
    : pacer_(clock, &packet_router_, event_log),
      send_side_cc_(clock, nullptr /* observer */, event_log, &pacer_) {}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return &send_side_cc_;
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpTransportControllerSend::keepalive_config() const {
  return keepalive_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  pacer_.SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);
}

void RtpTransportControllerSend::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}
Module* RtpTransportControllerSend::GetPacerModule() {
  return &pacer_;
}
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  pacer_.SetPacingFactor(pacing_factor);
}
void RtpTransportControllerSend::SetQueueTimeLimit(int limit_ms) {
  pacer_.SetQueueTimeLimit(limit_ms);
}
Module* RtpTransportControllerSend::GetModule() {
  return &send_side_cc_;
}
CallStatsObserver* RtpTransportControllerSend::GetCallStatsObserver() {
  return &send_side_cc_;
}
void RtpTransportControllerSend::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_.RegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_.DeRegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::RegisterNetworkObserver(
    NetworkChangedObserver* observer) {
  send_side_cc_.RegisterNetworkObserver(observer);
}
void RtpTransportControllerSend::DeRegisterNetworkObserver(
    NetworkChangedObserver* observer) {
  send_side_cc_.DeRegisterNetworkObserver(observer);
}
void RtpTransportControllerSend::SetBweBitrates(int min_bitrate_bps,
                                                int start_bitrate_bps,
                                                int max_bitrate_bps) {
  send_side_cc_.SetBweBitrates(min_bitrate_bps, start_bitrate_bps,
                               max_bitrate_bps);
}
void RtpTransportControllerSend::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int start_bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  send_side_cc_.OnNetworkRouteChanged(network_route, start_bitrate_bps,
                                      min_bitrate_bps, max_bitrate_bps);
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  send_side_cc_.SignalNetworkState(network_available ? kNetworkUp
                                                     : kNetworkDown);
}
void RtpTransportControllerSend::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  send_side_cc_.SetTransportOverhead(transport_overhead_bytes_per_packet);
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return send_side_cc_.GetBandwidthObserver();
}
bool RtpTransportControllerSend::AvailableBandwidth(uint32_t* bandwidth) const {
  return send_side_cc_.AvailableBandwidth(bandwidth);
}
int64_t RtpTransportControllerSend::GetPacerQueuingDelayMs() const {
  return send_side_cc_.GetPacerQueuingDelayMs();
}
int64_t RtpTransportControllerSend::GetFirstPacketTimeMs() const {
  return send_side_cc_.GetFirstPacketTimeMs();
}
RateLimiter* RtpTransportControllerSend::GetRetransmissionRateLimiter() {
  return send_side_cc_.GetRetransmissionRateLimiter();
}
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  send_side_cc_.EnablePeriodicAlrProbing(enable);
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  send_side_cc_.OnSentPacket(sent_packet);
}

}  // namespace webrtc
