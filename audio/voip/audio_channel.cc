/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_channel.h"

#include <utility>
#include <vector>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr int kRtcpReportIntervalMs = 5000;

}  // namespace

AudioChannel::AudioChannel(
    Transport* transport,
    uint32_t local_ssrc,
    TaskQueueFactory* task_queue_factory,
    ProcessThread* process_thread,
    AudioMixer* audio_mixer,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory)
    : audio_mixer_(audio_mixer), process_thread_(process_thread) {
  RTC_DCHECK(task_queue_factory);
  RTC_DCHECK(process_thread);
  RTC_DCHECK(audio_mixer);

  Clock* clock = Clock::GetRealTimeClock();
  receive_statistics_ = ReceiveStatistics::Create(clock);

  RtpRtcp::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.receive_statistics = receive_statistics_.get();
  rtp_config.rtcp_report_interval_ms = kRtcpReportIntervalMs;
  rtp_config.outgoing_transport = transport;
  rtp_config.local_media_ssrc = local_ssrc;

  rtp_rtcp_ = RtpRtcp::Create(rtp_config);

  rtp_rtcp_->SetSendingMediaStatus(false);
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  // ProcessThread periodically services RTP stack for RTCP.
  process_thread_->RegisterModule(rtp_rtcp_.get(), RTC_FROM_HERE);

  ingress_ = std::make_unique<AudioIngress>(rtp_rtcp_.get(), clock,
                                            receive_statistics_.get(),
                                            std::move(decoder_factory));
  egress_ =
      std::make_unique<AudioEgress>(rtp_rtcp_.get(), clock, task_queue_factory);

  // Set the instance of audio ingress to be part of audio mixer for ADM to
  // fetch audio samples to play.
  audio_mixer_->AddSource(ingress_.get());
}

AudioChannel::~AudioChannel() {
  if (egress_->IsSending()) {
    StopSend();
  }
  if (ingress_->IsPlaying()) {
    StopPlay();
  }

  audio_mixer_->RemoveSource(ingress_.get());
  process_thread_->DeRegisterModule(rtp_rtcp_.get());
}

void AudioChannel::StartSend() {
  egress_->StartSend();

  // Start sending with RTP stack if it has not been sending yet.
  if (!rtp_rtcp_->Sending() && rtp_rtcp_->SetSendingStatus(true) != 0) {
    RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to start sending";
  }
}

void AudioChannel::StopSend() {
  egress_->StopSend();

  // If the channel is not playing and RTP stack is active then deactivate RTP
  // stack. SetSendingStatus(false) triggers the transmission of RTCP BYE
  // message to remote endpoint.
  if (!IsPlaying() && rtp_rtcp_->Sending() &&
      rtp_rtcp_->SetSendingStatus(false) != 0) {
    RTC_DLOG(LS_ERROR) << "StopSend() RTP/RTCP failed to stop sending";
  }
}

void AudioChannel::StartPlay() {
  ingress_->StartPlay();

  // If RTP stack is not sending then start sending as in recv-only mode, RTCP
  // receiver report is expected.
  if (!rtp_rtcp_->Sending() && rtp_rtcp_->SetSendingStatus(true) != 0) {
    RTC_DLOG(LS_ERROR) << "StartPlay() RTP/RTCP failed to start sending";
  }
}

void AudioChannel::StopPlay() {
  ingress_->StopPlay();

  // Deactivate RTP stack only when both sending and receiving are stopped.
  if (!IsSendingMedia() && rtp_rtcp_->Sending() &&
      rtp_rtcp_->SetSendingStatus(false) != 0) {
    RTC_DLOG(LS_ERROR) << "StopPlay() RTP/RTCP failed to stop sending";
  }
}

}  // namespace webrtc
