/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video/audio_receive_stream.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

namespace webrtc {
VideoEncoder* VideoEncoder::Create(VideoEncoder::EncoderType codec_type) {
  switch (codec_type) {
    case kVp8:
      return VP8Encoder::Create();
    case kVp9:
      return VP9Encoder::Create();
  }
  RTC_NOTREACHED();
  return nullptr;
}

VideoDecoder* VideoDecoder::Create(VideoDecoder::DecoderType codec_type) {
  switch (codec_type) {
    case kVp8:
      return VP8Decoder::Create();
    case kVp9:
      return VP9Decoder::Create();
  }
  RTC_NOTREACHED();
  return nullptr;
}

const int Call::Config::kDefaultStartBitrateBps = 300000;

namespace internal {

class CpuOveruseObserverProxy : public webrtc::CpuOveruseObserver {
 public:
  explicit CpuOveruseObserverProxy(LoadObserver* overuse_callback)
      : crit_(CriticalSectionWrapper::CreateCriticalSection()),
        overuse_callback_(overuse_callback) {
    DCHECK(overuse_callback != nullptr);
  }

  virtual ~CpuOveruseObserverProxy() {}

  void OveruseDetected() override {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnLoadUpdate(LoadObserver::kOveruse);
  }

  void NormalUsage() override {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnLoadUpdate(LoadObserver::kUnderuse);
  }

 private:
  const rtc::scoped_ptr<CriticalSectionWrapper> crit_;
  LoadObserver* overuse_callback_ GUARDED_BY(crit_);
};

class Call : public webrtc::Call, public PacketReceiver {
 public:
  Call(webrtc::VideoEngine* video_engine, const Call::Config& config);
  virtual ~Call();

  PacketReceiver* Receiver() override;

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override;

  webrtc::VideoSendStream* CreateVideoSendStream(
      const webrtc::VideoSendStream::Config& config,
      const VideoEncoderConfig& encoder_config) override;
  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config) override;
  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;

  Stats GetStats() const override;

  DeliveryStatus DeliverPacket(MediaType media_type, const uint8_t* packet,
                               size_t length) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(NetworkState state) override;

 private:
  DeliveryStatus DeliverRtcp(MediaType media_type, const uint8_t* packet,
                             size_t length);
  DeliveryStatus DeliverRtp(MediaType media_type, const uint8_t* packet,
                            size_t length);

  Call::Config config_;

  // Needs to be held while write-locking |receive_crit_| or |send_crit_|. This
  // ensures that we have a consistent network state signalled to all senders
  // and receivers.
  rtc::scoped_ptr<CriticalSectionWrapper> network_enabled_crit_;
  bool network_enabled_ GUARDED_BY(network_enabled_crit_);

  rtc::scoped_ptr<RWLockWrapper> receive_crit_;
  std::map<uint32_t, AudioReceiveStream*> audio_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::map<uint32_t, VideoReceiveStream*> video_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::set<VideoReceiveStream*> video_receive_streams_
      GUARDED_BY(receive_crit_);

  rtc::scoped_ptr<RWLockWrapper> send_crit_;
  std::map<uint32_t, VideoSendStream*> video_send_ssrcs_ GUARDED_BY(send_crit_);
  std::set<VideoSendStream*> video_send_streams_ GUARDED_BY(send_crit_);

  rtc::scoped_ptr<CpuOveruseObserverProxy> overuse_observer_proxy_;

  VideoSendStream::RtpStateMap suspended_video_send_ssrcs_;

  VideoEngine* video_engine_;
  ViERTP_RTCP* rtp_rtcp_;
  ViECodec* codec_;
  ViERender* render_;
  ViEBase* base_;
  ViENetwork* network_;
  int base_channel_id_;
  ChannelGroup* channel_group_;

  rtc::scoped_ptr<VideoRender> external_render_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

Call* Call::Create(const Call::Config& config) {
  VideoEngine* video_engine = VideoEngine::Create();
  DCHECK(video_engine != nullptr);

  return new internal::Call(video_engine, config);
}

namespace internal {

Call::Call(webrtc::VideoEngine* video_engine, const Call::Config& config)
    : config_(config),
      network_enabled_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      network_enabled_(true),
      receive_crit_(RWLockWrapper::CreateRWLock()),
      send_crit_(RWLockWrapper::CreateRWLock()),
      video_engine_(video_engine),
      base_channel_id_(-1),
      external_render_(
          VideoRender::CreateVideoRender(42, nullptr, false, kRenderExternal)) {
  DCHECK(video_engine != nullptr);
  DCHECK(config.send_transport != nullptr);

  DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  DCHECK_GE(config.bitrate_config.start_bitrate_bps,
            config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    DCHECK_GE(config.bitrate_config.max_bitrate_bps,
              config.bitrate_config.start_bitrate_bps);
  }

  if (config.overuse_callback) {
    overuse_observer_proxy_.reset(
        new CpuOveruseObserverProxy(config.overuse_callback));
  }

  render_ = ViERender::GetInterface(video_engine_);
  DCHECK(render_ != nullptr);

  render_->RegisterVideoRenderModule(*external_render_.get());

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  DCHECK(rtp_rtcp_ != nullptr);

  codec_ = ViECodec::GetInterface(video_engine_);
  DCHECK(codec_ != nullptr);

  network_ = ViENetwork::GetInterface(video_engine_);

  // As a workaround for non-existing calls in the old API, create a base
  // channel used as default channel when creating send and receive streams.
  base_ = ViEBase::GetInterface(video_engine_);
  DCHECK(base_ != nullptr);

  base_->CreateChannel(base_channel_id_);
  DCHECK(base_channel_id_ != -1);
  channel_group_ = base_->GetChannelGroup(base_channel_id_);

  network_->SetBitrateConfig(base_channel_id_,
                             config_.bitrate_config.min_bitrate_bps,
                             config_.bitrate_config.start_bitrate_bps,
                             config_.bitrate_config.max_bitrate_bps);
}

Call::~Call() {
  CHECK_EQ(0u, video_send_ssrcs_.size());
  CHECK_EQ(0u, video_send_streams_.size());
  CHECK_EQ(0u, audio_receive_ssrcs_.size());
  CHECK_EQ(0u, video_receive_ssrcs_.size());
  CHECK_EQ(0u, video_receive_streams_.size());
  base_->DeleteChannel(base_channel_id_);

  render_->DeRegisterVideoRenderModule(*external_render_.get());

  base_->Release();
  network_->Release();
  codec_->Release();
  render_->Release();
  rtp_rtcp_->Release();
  CHECK(webrtc::VideoEngine::Delete(video_engine_));
}

PacketReceiver* Call::Receiver() { return this; }

webrtc::AudioReceiveStream* Call::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioReceiveStream");
  LOG(LS_INFO) << "CreateAudioReceiveStream: " << config.ToString();
  AudioReceiveStream* receive_stream = new AudioReceiveStream(
      channel_group_->GetRemoteBitrateEstimator(), config);
  {
    WriteLockScoped write_lock(*receive_crit_);
    DCHECK(audio_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
        audio_receive_ssrcs_.end());
    audio_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  }
  return receive_stream;
}

void Call::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioReceiveStream");
  DCHECK(receive_stream != nullptr);
  AudioReceiveStream* audio_receive_stream =
      static_cast<AudioReceiveStream*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);
    size_t num_deleted = audio_receive_ssrcs_.erase(
        audio_receive_stream->config().rtp.remote_ssrc);
    DCHECK(num_deleted == 1);
  }
  delete audio_receive_stream;
}

webrtc::VideoSendStream* Call::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoSendStream");
  LOG(LS_INFO) << "CreateVideoSendStream: " << config.ToString();
  DCHECK(!config.rtp.ssrcs.empty());

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  VideoSendStream* send_stream =
      new VideoSendStream(config_.send_transport, overuse_observer_proxy_.get(),
                          video_engine_, channel_group_, config, encoder_config,
                          suspended_video_send_ssrcs_, base_channel_id_);

  // This needs to be taken before send_crit_ as both locks need to be held
  // while changing network state.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  WriteLockScoped write_lock(*send_crit_);
  for (uint32_t ssrc : config.rtp.ssrcs) {
    DCHECK(video_send_ssrcs_.find(ssrc) == video_send_ssrcs_.end());
    video_send_ssrcs_[ssrc] = send_stream;
  }
  video_send_streams_.insert(send_stream);

  if (!network_enabled_)
    send_stream->SignalNetworkState(kNetworkDown);
  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoSendStream");
  DCHECK(send_stream != nullptr);

  send_stream->Stop();

  VideoSendStream* send_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*send_crit_);
    auto it = video_send_ssrcs_.begin();
    while (it != video_send_ssrcs_.end()) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        video_send_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_send_streams_.erase(send_stream_impl);
  }
  CHECK(send_stream_impl != nullptr);

  VideoSendStream::RtpStateMap rtp_state = send_stream_impl->GetRtpStates();

  for (VideoSendStream::RtpStateMap::iterator it = rtp_state.begin();
       it != rtp_state.end();
       ++it) {
    suspended_video_send_ssrcs_[it->first] = it->second;
  }

  delete send_stream_impl;
}

webrtc::VideoReceiveStream* Call::CreateVideoReceiveStream(
    const webrtc::VideoReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoReceiveStream");
  LOG(LS_INFO) << "CreateVideoReceiveStream: " << config.ToString();
  VideoReceiveStream* receive_stream = new VideoReceiveStream(
      video_engine_, channel_group_, config, config_.send_transport,
      config_.voice_engine, base_channel_id_);

  // This needs to be taken before receive_crit_ as both locks need to be held
  // while changing network state.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  WriteLockScoped write_lock(*receive_crit_);
  DCHECK(video_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
      video_receive_ssrcs_.end());
  video_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    video_receive_ssrcs_[it->second.ssrc] = receive_stream;
  video_receive_streams_.insert(receive_stream);

  if (!network_enabled_)
    receive_stream->SignalNetworkState(kNetworkDown);
  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  DCHECK(receive_stream != nullptr);

  VideoReceiveStream* receive_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    auto it = video_receive_ssrcs_.begin();
    while (it != video_receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        if (receive_stream_impl != nullptr)
          DCHECK(receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        video_receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_receive_streams_.erase(receive_stream_impl);
  }
  CHECK(receive_stream_impl != nullptr);
  delete receive_stream_impl;
}

Call::Stats Call::GetStats() const {
  Stats stats;
  // Ignoring return values.
  uint32_t send_bandwidth = 0;
  rtp_rtcp_->GetEstimatedSendBandwidth(base_channel_id_, &send_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
  uint32_t recv_bandwidth = 0;
  rtp_rtcp_->GetEstimatedReceiveBandwidth(base_channel_id_, &recv_bandwidth);
  stats.recv_bandwidth_bps = recv_bandwidth;
  stats.pacer_delay_ms = channel_group_->GetPacerQueuingDelayMs();
  {
    ReadLockScoped read_lock(*send_crit_);
    for (const auto& kv : video_send_ssrcs_) {
      int rtt_ms = kv.second->GetRtt();
      if (rtt_ms > 0)
        stats.rtt_ms = rtt_ms;
    }
  }
  return stats;
}

void Call::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  TRACE_EVENT0("webrtc", "Call::SetBitrateConfig");
  DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1)
    DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
  if (config_.bitrate_config.min_bitrate_bps ==
          bitrate_config.min_bitrate_bps &&
      (bitrate_config.start_bitrate_bps <= 0 ||
       config_.bitrate_config.start_bitrate_bps ==
           bitrate_config.start_bitrate_bps) &&
      config_.bitrate_config.max_bitrate_bps ==
          bitrate_config.max_bitrate_bps) {
    // Nothing new to set, early abort to avoid encoder reconfigurations.
    return;
  }
  config_.bitrate_config = bitrate_config;
  network_->SetBitrateConfig(base_channel_id_, bitrate_config.min_bitrate_bps,
                             bitrate_config.start_bitrate_bps,
                             bitrate_config.max_bitrate_bps);
}

void Call::SignalNetworkState(NetworkState state) {
  // Take crit for entire function, it needs to be held while updating streams
  // to guarantee a consistent state across streams.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  network_enabled_ = state == kNetworkUp;
  {
    ReadLockScoped write_lock(*send_crit_);
    for (auto& kv : video_send_ssrcs_) {
      kv.second->SignalNetworkState(state);
    }
  }
  {
    ReadLockScoped write_lock(*receive_crit_);
    for (auto& kv : video_receive_ssrcs_) {
      kv.second->SignalNetworkState(state);
    }
  }
}

PacketReceiver::DeliveryStatus Call::DeliverRtcp(MediaType media_type,
                                                 const uint8_t* packet,
                                                 size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  //             Return DELIVERY_UNKNOWN_SSRC if it can be determined that
  //             there's no receiver of the packet.
  bool rtcp_delivered = false;
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*receive_crit_);
    for (VideoReceiveStream* stream : video_receive_streams_) {
      if (stream->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*send_crit_);
    for (VideoSendStream* stream : video_send_streams_) {
      if (stream->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(MediaType media_type,
                                                const uint8_t* packet,
                                                size_t length) {
  // Minimum RTP header size.
  if (length < 12)
    return DELIVERY_PACKET_ERROR;

  uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&packet[8]);

  ReadLockScoped read_lock(*receive_crit_);
  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    auto it = audio_receive_ssrcs_.find(ssrc);
    if (it != audio_receive_ssrcs_.end()) {
      return it->second->DeliverRtp(packet, length) ? DELIVERY_OK
                                                    : DELIVERY_PACKET_ERROR;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    auto it = video_receive_ssrcs_.find(ssrc);
    if (it != video_receive_ssrcs_.end()) {
      return it->second->DeliverRtp(packet, length) ? DELIVERY_OK
                                                    : DELIVERY_PACKET_ERROR;
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(MediaType media_type,
                                                   const uint8_t* packet,
                                                   size_t length) {
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(media_type, packet, length);

  return DeliverRtp(media_type, packet, length);
}

}  // namespace internal
}  // namespace webrtc
