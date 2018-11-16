/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send_proxy.h"

#include <utility>

#include "api/crypto/frameencryptorinterface.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace voe {
ChannelSendProxy::ChannelSendProxy() {}

ChannelSendProxy::ChannelSendProxy(std::unique_ptr<ChannelSend> channel)
    : channel_(std::move(channel)) {
  RTC_DCHECK(channel_);
  module_process_thread_checker_.DetachFromThread();
}

ChannelSendProxy::~ChannelSendProxy() {}

void ChannelSendProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->SetLocalSSRC(ssrc);
}

void ChannelSendProxy::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetNACKStatus(enable, max_packets);
}

CallSendStatistics ChannelSendProxy::GetRTCPStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetRTCPStatistics();
}

void ChannelSendProxy::RegisterTransport(Transport* transport) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->RegisterTransport(transport);
}

bool ChannelSendProxy::ReceivedRTCPPacket(const uint8_t* packet,
                                          size_t length) {
  // May be called on either worker thread or network thread.
  return channel_->ReceivedRTCPPacket(packet, length);
}

bool ChannelSendProxy::SetEncoder(int payload_type,
                                  std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->SetEncoder(payload_type, std::move(encoder));
}

void ChannelSendProxy::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->ModifyEncoder(modifier);
}

void ChannelSendProxy::SetRTCPStatus(bool enable) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetRTCPStatus(enable);
}

void ChannelSendProxy::SetMid(const std::string& mid, int extension_id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetMid(mid, extension_id);
}

void ChannelSendProxy::SetRTCP_CNAME(absl::string_view c_name) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetRTCP_CNAME(c_name);
}

void ChannelSendProxy::SetExtmapAllowMixed(bool extmap_allow_mixed) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetExtmapAllowMixed(extmap_allow_mixed);
}

void ChannelSendProxy::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetSendAudioLevelIndicationStatus(enable, id);
}

void ChannelSendProxy::EnableSendTransportSequenceNumber(int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->EnableSendTransportSequenceNumber(id);
}

void ChannelSendProxy::RegisterSenderCongestionControlObjects(
    RtpTransportControllerSendInterface* transport,
    RtcpBandwidthObserver* bandwidth_observer) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->RegisterSenderCongestionControlObjects(transport,
                                                   bandwidth_observer);
}

void ChannelSendProxy::ResetSenderCongestionControlObjects() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->ResetSenderCongestionControlObjects();
}

std::vector<ReportBlock> ChannelSendProxy::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetRemoteRTCPReportBlocks();
}

ANAStats ChannelSendProxy::GetANAStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetANAStatistics();
}

bool ChannelSendProxy::SetSendTelephoneEventPayloadType(int payload_type,
                                                        int payload_frequency) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->SetSendTelephoneEventPayloadType(payload_type,
                                                    payload_frequency);
}

bool ChannelSendProxy::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->SendTelephoneEventOutband(event, duration_ms);
}

void ChannelSendProxy::SetBitrate(int bitrate_bps,
                                  int64_t probing_interval_ms) {
  // This method can be called on the worker thread, module process thread
  // or on a TaskQueue via VideoSendStreamImpl::OnEncoderConfigurationChanged.
  // TODO(solenberg): Figure out a good way to check this or enforce calling
  // rules.
  // RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
  //            module_process_thread_checker_.CalledOnValidThread());
  channel_->SetBitRate(bitrate_bps, probing_interval_ms);
}

int ChannelSendProxy::GetBitrate() const {
  return channel_->GetBitRate();
}

void ChannelSendProxy::SetInputMute(bool muted) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetInputMute(muted);
}

void ChannelSendProxy::ProcessAndEncodeAudio(
    std::unique_ptr<AudioFrame> audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel_->ProcessAndEncodeAudio(std::move(audio_frame));
}

void ChannelSendProxy::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetTransportOverhead(transport_overhead_per_packet);
}

RtpRtcp* ChannelSendProxy::GetRtpRtcp() const {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  return channel_->GetRtpRtcp();
}

void ChannelSendProxy::OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->OnTwccBasedUplinkPacketLossRate(packet_loss_rate);
}

void ChannelSendProxy::OnRecoverableUplinkPacketLossRate(
    float recoverable_packet_loss_rate) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->OnRecoverableUplinkPacketLossRate(recoverable_packet_loss_rate);
}

void ChannelSendProxy::StartSend() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->StartSend();
}

void ChannelSendProxy::StopSend() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->StopSend();
}

ChannelSend* ChannelSendProxy::GetChannel() const {
  return channel_.get();
}

void ChannelSendProxy::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetFrameEncryptor(frame_encryptor);
}

}  // namespace voe
}  // namespace webrtc
