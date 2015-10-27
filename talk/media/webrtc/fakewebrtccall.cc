/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/webrtc/fakewebrtccall.h"

#include <algorithm>

#include "talk/media/base/rtputils.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"

namespace cricket {
FakeAudioSendStream::FakeAudioSendStream(
    const webrtc::AudioSendStream::Config& config) : config_(config) {
  RTC_DCHECK(config.voe_channel_id != -1);
}

void FakeAudioSendStream::SetStats(
    const webrtc::AudioSendStream::Stats& stats) {
  stats_ = stats;
}

const webrtc::AudioSendStream::Config&
    FakeAudioSendStream::GetConfig() const {
  return config_;
}

webrtc::AudioSendStream::Stats FakeAudioSendStream::GetStats() const {
  return stats_;
}

FakeAudioReceiveStream::FakeAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config)
    : config_(config), received_packets_(0) {
  RTC_DCHECK(config.voe_channel_id != -1);
}

const webrtc::AudioReceiveStream::Config&
    FakeAudioReceiveStream::GetConfig() const {
  return config_;
}

void FakeAudioReceiveStream::SetStats(
    const webrtc::AudioReceiveStream::Stats& stats) {
  stats_ = stats;
}

void FakeAudioReceiveStream::IncrementReceivedPackets() {
  received_packets_++;
}

webrtc::AudioReceiveStream::Stats FakeAudioReceiveStream::GetStats() const {
  return stats_;
}

FakeVideoSendStream::FakeVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const webrtc::VideoEncoderConfig& encoder_config)
    : sending_(false),
      config_(config),
      codec_settings_set_(false),
      num_swapped_frames_(0) {
  RTC_DCHECK(config.encoder_settings.encoder != NULL);
  ReconfigureVideoEncoder(encoder_config);
}

webrtc::VideoSendStream::Config FakeVideoSendStream::GetConfig() const {
  return config_;
}

webrtc::VideoEncoderConfig FakeVideoSendStream::GetEncoderConfig() const {
  return encoder_config_;
}

std::vector<webrtc::VideoStream> FakeVideoSendStream::GetVideoStreams() {
  return encoder_config_.streams;
}

bool FakeVideoSendStream::IsSending() const {
  return sending_;
}

bool FakeVideoSendStream::GetVp8Settings(
    webrtc::VideoCodecVP8* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = vpx_settings_.vp8;
  return true;
}

bool FakeVideoSendStream::GetVp9Settings(
    webrtc::VideoCodecVP9* settings) const {
  if (!codec_settings_set_) {
    return false;
  }

  *settings = vpx_settings_.vp9;
  return true;
}

int FakeVideoSendStream::GetNumberOfSwappedFrames() const {
  return num_swapped_frames_;
}

int FakeVideoSendStream::GetLastWidth() const {
  return last_frame_.width();
}

int FakeVideoSendStream::GetLastHeight() const {
  return last_frame_.height();
}

int64_t FakeVideoSendStream::GetLastTimestamp() const {
  RTC_DCHECK(last_frame_.ntp_time_ms() == 0);
  return last_frame_.render_time_ms();
}

void FakeVideoSendStream::IncomingCapturedFrame(
    const webrtc::VideoFrame& frame) {
  ++num_swapped_frames_;
  last_frame_.ShallowCopy(frame);
}

void FakeVideoSendStream::SetStats(
    const webrtc::VideoSendStream::Stats& stats) {
  stats_ = stats;
}

webrtc::VideoSendStream::Stats FakeVideoSendStream::GetStats() {
  return stats_;
}

bool FakeVideoSendStream::ReconfigureVideoEncoder(
    const webrtc::VideoEncoderConfig& config) {
  encoder_config_ = config;
  if (config.encoder_specific_settings != NULL) {
    if (config_.encoder_settings.payload_name == "VP8") {
      vpx_settings_.vp8 = *reinterpret_cast<const webrtc::VideoCodecVP8*>(
                              config.encoder_specific_settings);
    } else if (config_.encoder_settings.payload_name == "VP9") {
      vpx_settings_.vp9 = *reinterpret_cast<const webrtc::VideoCodecVP9*>(
                              config.encoder_specific_settings);
    } else {
      ADD_FAILURE() << "Unsupported encoder payload: "
                    << config_.encoder_settings.payload_name;
    }
  }
  codec_settings_set_ = config.encoder_specific_settings != NULL;
  return true;
}

webrtc::VideoCaptureInput* FakeVideoSendStream::Input() {
  return this;
}

void FakeVideoSendStream::Start() {
  sending_ = true;
}

void FakeVideoSendStream::Stop() {
  sending_ = false;
}

FakeVideoReceiveStream::FakeVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config)
    : config_(config), receiving_(false) {
}

webrtc::VideoReceiveStream::Config FakeVideoReceiveStream::GetConfig() {
  return config_;
}

bool FakeVideoReceiveStream::IsReceiving() const {
  return receiving_;
}

void FakeVideoReceiveStream::InjectFrame(const webrtc::VideoFrame& frame,
                                         int time_to_render_ms) {
  config_.renderer->RenderFrame(frame, time_to_render_ms);
}

webrtc::VideoReceiveStream::Stats FakeVideoReceiveStream::GetStats() const {
  return stats_;
}

void FakeVideoReceiveStream::Start() {
  receiving_ = true;
}

void FakeVideoReceiveStream::Stop() {
  receiving_ = false;
}

void FakeVideoReceiveStream::SetStats(
    const webrtc::VideoReceiveStream::Stats& stats) {
  stats_ = stats;
}

FakeCall::FakeCall(const webrtc::Call::Config& config)
    : config_(config),
      network_state_(webrtc::kNetworkUp),
      num_created_send_streams_(0),
      num_created_receive_streams_(0) {}

FakeCall::~FakeCall() {
  EXPECT_EQ(0u, video_send_streams_.size());
  EXPECT_EQ(0u, audio_send_streams_.size());
  EXPECT_EQ(0u, video_receive_streams_.size());
  EXPECT_EQ(0u, audio_receive_streams_.size());
}

webrtc::Call::Config FakeCall::GetConfig() const {
  return config_;
}

const std::vector<FakeVideoSendStream*>& FakeCall::GetVideoSendStreams() {
  return video_send_streams_;
}

const std::vector<FakeVideoReceiveStream*>& FakeCall::GetVideoReceiveStreams() {
  return video_receive_streams_;
}

const std::vector<FakeAudioSendStream*>& FakeCall::GetAudioSendStreams() {
  return audio_send_streams_;
}

const FakeAudioSendStream* FakeCall::GetAudioSendStream(uint32_t ssrc) {
  for (const auto* p : GetAudioSendStreams()) {
    if (p->GetConfig().rtp.ssrc == ssrc) {
      return p;
    }
  }
  return nullptr;
}

const std::vector<FakeAudioReceiveStream*>& FakeCall::GetAudioReceiveStreams() {
  return audio_receive_streams_;
}

const FakeAudioReceiveStream* FakeCall::GetAudioReceiveStream(uint32_t ssrc) {
  for (const auto* p : GetAudioReceiveStreams()) {
    if (p->GetConfig().rtp.remote_ssrc == ssrc) {
      return p;
    }
  }
  return nullptr;
}

webrtc::NetworkState FakeCall::GetNetworkState() const {
  return network_state_;
}

webrtc::AudioSendStream* FakeCall::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  FakeAudioSendStream* fake_stream = new FakeAudioSendStream(config);
  audio_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  auto it = std::find(audio_send_streams_.begin(),
                      audio_send_streams_.end(),
                      static_cast<FakeAudioSendStream*>(send_stream));
  if (it == audio_send_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioSendStream called with unknown paramter.";
  } else {
    delete *it;
    audio_send_streams_.erase(it);
  }
}

webrtc::AudioReceiveStream* FakeCall::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  audio_receive_streams_.push_back(new FakeAudioReceiveStream(config));
  ++num_created_receive_streams_;
  return audio_receive_streams_.back();
}

void FakeCall::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  auto it = std::find(audio_receive_streams_.begin(),
                      audio_receive_streams_.end(),
                      static_cast<FakeAudioReceiveStream*>(receive_stream));
  if (it == audio_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyAudioReceiveStream called with unknown paramter.";
  } else {
    delete *it;
    audio_receive_streams_.erase(it);
  }
}

webrtc::VideoSendStream* FakeCall::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const webrtc::VideoEncoderConfig& encoder_config) {
  FakeVideoSendStream* fake_stream =
      new FakeVideoSendStream(config, encoder_config);
  video_send_streams_.push_back(fake_stream);
  ++num_created_send_streams_;
  return fake_stream;
}

void FakeCall::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  auto it = std::find(video_send_streams_.begin(),
                      video_send_streams_.end(),
                      static_cast<FakeVideoSendStream*>(send_stream));
  if (it == video_send_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoSendStream called with unknown paramter.";
  } else {
    delete *it;
    video_send_streams_.erase(it);
  }
}

webrtc::VideoReceiveStream* FakeCall::CreateVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config) {
  video_receive_streams_.push_back(new FakeVideoReceiveStream(config));
  ++num_created_receive_streams_;
  return video_receive_streams_.back();
}

void FakeCall::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  auto it = std::find(video_receive_streams_.begin(),
                      video_receive_streams_.end(),
                      static_cast<FakeVideoReceiveStream*>(receive_stream));
  if (it == video_receive_streams_.end()) {
    ADD_FAILURE() << "DestroyVideoReceiveStream called with unknown paramter.";
  } else {
    delete *it;
    video_receive_streams_.erase(it);
  }
}

webrtc::PacketReceiver* FakeCall::Receiver() {
  return this;
}

FakeCall::DeliveryStatus FakeCall::DeliverPacket(
    webrtc::MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const webrtc::PacketTime& packet_time) {
  EXPECT_GE(length, 12u);
  uint32_t ssrc;
  if (!GetRtpSsrc(packet, length, &ssrc))
    return DELIVERY_PACKET_ERROR;

  if (media_type == webrtc::MediaType::ANY ||
      media_type == webrtc::MediaType::VIDEO) {
    for (auto receiver : video_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc)
        return DELIVERY_OK;
    }
  }
  if (media_type == webrtc::MediaType::ANY ||
      media_type == webrtc::MediaType::AUDIO) {
    for (auto receiver : audio_receive_streams_) {
      if (receiver->GetConfig().rtp.remote_ssrc == ssrc) {
        receiver->IncrementReceivedPackets();
        return DELIVERY_OK;
      }
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

void FakeCall::SetStats(const webrtc::Call::Stats& stats) {
  stats_ = stats;
}

int FakeCall::GetNumCreatedSendStreams() const {
  return num_created_send_streams_;
}

int FakeCall::GetNumCreatedReceiveStreams() const {
  return num_created_receive_streams_;
}

webrtc::Call::Stats FakeCall::GetStats() const {
  return stats_;
}

void FakeCall::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  config_.bitrate_config = bitrate_config;
}

void FakeCall::SignalNetworkState(webrtc::NetworkState state) {
  network_state_ = state;
}

void FakeCall::OnSentPacket(const rtc::SentPacket& sent_packet) {
  last_sent_packet_ = sent_packet;
}
}  // namespace cricket
