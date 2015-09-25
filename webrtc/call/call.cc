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

#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/call/rtc_event_log.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/cpu_info.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/voice_engine/include/voe_codec.h"

namespace webrtc {

const int Call::Config::kDefaultStartBitrateBps = 300000;

namespace internal {

class Call : public webrtc::Call, public PacketReceiver {
 public:
  explicit Call(const Call::Config& config);
  virtual ~Call();

  PacketReceiver* Receiver() override;

  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override;

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

  DeliveryStatus DeliverPacket(MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(NetworkState state) override;

 private:
  DeliveryStatus DeliverRtcp(MediaType media_type, const uint8_t* packet,
                             size_t length);
  DeliveryStatus DeliverRtp(MediaType media_type,
                            const uint8_t* packet,
                            size_t length,
                            const PacketTime& packet_time);

  void SetBitrateControllerConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config);

  void ConfigureSync(const std::string& sync_group)
      EXCLUSIVE_LOCKS_REQUIRED(receive_crit_);

  const int num_cpu_cores_;
  const rtc::scoped_ptr<ProcessThread> module_process_thread_;
  const rtc::scoped_ptr<ChannelGroup> channel_group_;
  volatile int next_channel_id_;
  Call::Config config_;

  // Needs to be held while write-locking |receive_crit_| or |send_crit_|. This
  // ensures that we have a consistent network state signalled to all senders
  // and receivers.
  rtc::CriticalSection network_enabled_crit_;
  bool network_enabled_ GUARDED_BY(network_enabled_crit_);

  rtc::scoped_ptr<RWLockWrapper> receive_crit_;
  std::map<uint32_t, AudioReceiveStream*> audio_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::map<uint32_t, VideoReceiveStream*> video_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::set<VideoReceiveStream*> video_receive_streams_
      GUARDED_BY(receive_crit_);
  std::map<std::string, AudioReceiveStream*> sync_stream_mapping_
      GUARDED_BY(receive_crit_);

  rtc::scoped_ptr<RWLockWrapper> send_crit_;
  std::map<uint32_t, VideoSendStream*> video_send_ssrcs_ GUARDED_BY(send_crit_);
  std::set<VideoSendStream*> video_send_streams_ GUARDED_BY(send_crit_);

  VideoSendStream::RtpStateMap suspended_video_send_ssrcs_;

  RtcEventLog* event_log_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

Call* Call::Create(const Call::Config& config) {
  return new internal::Call(config);
}

namespace internal {

Call::Call(const Call::Config& config)
    : num_cpu_cores_(CpuInfo::DetectNumberOfCores()),
      module_process_thread_(ProcessThread::Create("ModuleProcessThread")),
      channel_group_(new ChannelGroup(module_process_thread_.get())),
      next_channel_id_(0),
      config_(config),
      network_enabled_(true),
      receive_crit_(RWLockWrapper::CreateRWLock()),
      send_crit_(RWLockWrapper::CreateRWLock()),
      event_log_(nullptr) {
  RTC_DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  RTC_DCHECK_GE(config.bitrate_config.start_bitrate_bps,
                config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    RTC_DCHECK_GE(config.bitrate_config.max_bitrate_bps,
                  config.bitrate_config.start_bitrate_bps);
  }
  if (config.voice_engine) {
    VoECodec* voe_codec = VoECodec::GetInterface(config.voice_engine);
    if (voe_codec) {
      event_log_ = voe_codec->GetEventLog();
      voe_codec->Release();
    }
  }

  Trace::CreateTrace();
  module_process_thread_->Start();

  SetBitrateControllerConfig(config_.bitrate_config);
}

Call::~Call() {
  RTC_CHECK_EQ(0u, video_send_ssrcs_.size());
  RTC_CHECK_EQ(0u, video_send_streams_.size());
  RTC_CHECK_EQ(0u, audio_receive_ssrcs_.size());
  RTC_CHECK_EQ(0u, video_receive_ssrcs_.size());
  RTC_CHECK_EQ(0u, video_receive_streams_.size());

  module_process_thread_->Stop();
  Trace::ReturnTrace();
}

PacketReceiver* Call::Receiver() { return this; }

webrtc::AudioSendStream* Call::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  return nullptr;
}

void Call::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
}

webrtc::AudioReceiveStream* Call::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioReceiveStream");
  LOG(LS_INFO) << "CreateAudioReceiveStream: " << config.ToString();
  AudioReceiveStream* receive_stream = new AudioReceiveStream(
      channel_group_->GetRemoteBitrateEstimator(), config);
  {
    WriteLockScoped write_lock(*receive_crit_);
    RTC_DCHECK(audio_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
               audio_receive_ssrcs_.end());
    audio_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
    ConfigureSync(config.sync_group);
  }
  return receive_stream;
}

void Call::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioReceiveStream");
  RTC_DCHECK(receive_stream != nullptr);
  AudioReceiveStream* audio_receive_stream =
      static_cast<AudioReceiveStream*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);
    size_t num_deleted = audio_receive_ssrcs_.erase(
        audio_receive_stream->config().rtp.remote_ssrc);
    RTC_DCHECK(num_deleted == 1);
    const std::string& sync_group = audio_receive_stream->config().sync_group;
    const auto it = sync_stream_mapping_.find(sync_group);
    if (it != sync_stream_mapping_.end() &&
        it->second == audio_receive_stream) {
      sync_stream_mapping_.erase(it);
      ConfigureSync(sync_group);
    }
  }
  delete audio_receive_stream;
}

webrtc::VideoSendStream* Call::CreateVideoSendStream(
    const webrtc::VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoSendStream");
  LOG(LS_INFO) << "CreateVideoSendStream: " << config.ToString();
  RTC_DCHECK(!config.rtp.ssrcs.empty());

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  VideoSendStream* send_stream = new VideoSendStream(num_cpu_cores_,
      module_process_thread_.get(), channel_group_.get(),
      rtc::AtomicOps::Increment(&next_channel_id_), config, encoder_config,
      suspended_video_send_ssrcs_);

  // This needs to be taken before send_crit_ as both locks need to be held
  // while changing network state.
  rtc::CritScope lock(&network_enabled_crit_);
  WriteLockScoped write_lock(*send_crit_);
  for (uint32_t ssrc : config.rtp.ssrcs) {
    RTC_DCHECK(video_send_ssrcs_.find(ssrc) == video_send_ssrcs_.end());
    video_send_ssrcs_[ssrc] = send_stream;
  }
  video_send_streams_.insert(send_stream);

  if (event_log_)
    event_log_->LogVideoSendStreamConfig(config);

  if (!network_enabled_)
    send_stream->SignalNetworkState(kNetworkDown);
  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoSendStream");
  RTC_DCHECK(send_stream != nullptr);

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
  RTC_CHECK(send_stream_impl != nullptr);

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
      num_cpu_cores_, channel_group_.get(),
      rtc::AtomicOps::Increment(&next_channel_id_), config,
      config_.voice_engine);

  // This needs to be taken before receive_crit_ as both locks need to be held
  // while changing network state.
  rtc::CritScope lock(&network_enabled_crit_);
  WriteLockScoped write_lock(*receive_crit_);
  RTC_DCHECK(video_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
             video_receive_ssrcs_.end());
  video_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    video_receive_ssrcs_[it->second.ssrc] = receive_stream;
  video_receive_streams_.insert(receive_stream);

  ConfigureSync(config.sync_group);

  if (!network_enabled_)
    receive_stream->SignalNetworkState(kNetworkDown);

  if (event_log_)
    event_log_->LogVideoReceiveStreamConfig(config);

  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  RTC_DCHECK(receive_stream != nullptr);
  VideoReceiveStream* receive_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    auto it = video_receive_ssrcs_.begin();
    while (it != video_receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        if (receive_stream_impl != nullptr)
          RTC_DCHECK(receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        video_receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_receive_streams_.erase(receive_stream_impl);
    RTC_CHECK(receive_stream_impl != nullptr);
    ConfigureSync(receive_stream_impl->config().sync_group);
  }
  delete receive_stream_impl;
}

Call::Stats Call::GetStats() const {
  Stats stats;
  // Fetch available send/receive bitrates.
  uint32_t send_bandwidth = 0;
  channel_group_->GetBitrateController()->AvailableBandwidth(&send_bandwidth);
  std::vector<unsigned int> ssrcs;
  uint32_t recv_bandwidth = 0;
  channel_group_->GetRemoteBitrateEstimator()->LatestEstimate(&ssrcs,
                                                              &recv_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
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
  RTC_DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1)
    RTC_DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
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
  SetBitrateControllerConfig(bitrate_config);
}

void Call::SetBitrateControllerConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  BitrateController* bitrate_controller =
      channel_group_->GetBitrateController();
  if (bitrate_config.start_bitrate_bps > 0)
    bitrate_controller->SetStartBitrate(bitrate_config.start_bitrate_bps);
  bitrate_controller->SetMinMaxBitrate(bitrate_config.min_bitrate_bps,
                                       bitrate_config.max_bitrate_bps);
}

void Call::SignalNetworkState(NetworkState state) {
  // Take crit for entire function, it needs to be held while updating streams
  // to guarantee a consistent state across streams.
  rtc::CritScope lock(&network_enabled_crit_);
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

void Call::ConfigureSync(const std::string& sync_group) {
  // Set sync only if there was no previous one.
  if (config_.voice_engine == nullptr || sync_group.empty())
    return;

  AudioReceiveStream* sync_audio_stream = nullptr;
  // Find existing audio stream.
  const auto it = sync_stream_mapping_.find(sync_group);
  if (it != sync_stream_mapping_.end()) {
    sync_audio_stream = it->second;
  } else {
    // No configured audio stream, see if we can find one.
    for (const auto& kv : audio_receive_ssrcs_) {
      if (kv.second->config().sync_group == sync_group) {
        if (sync_audio_stream != nullptr) {
          LOG(LS_WARNING) << "Attempting to sync more than one audio stream "
                             "within the same sync group. This is not "
                             "supported in the current implementation.";
          break;
        }
        sync_audio_stream = kv.second;
      }
    }
  }
  if (sync_audio_stream)
    sync_stream_mapping_[sync_group] = sync_audio_stream;
  size_t num_synced_streams = 0;
  for (VideoReceiveStream* video_stream : video_receive_streams_) {
    if (video_stream->config().sync_group != sync_group)
      continue;
    ++num_synced_streams;
    if (num_synced_streams > 1) {
      // TODO(pbos): Support synchronizing more than one A/V pair.
      // https://code.google.com/p/webrtc/issues/detail?id=4762
      LOG(LS_WARNING) << "Attempting to sync more than one audio/video pair "
                         "within the same sync group. This is not supported in "
                         "the current implementation.";
    }
    // Only sync the first A/V pair within this sync group.
    if (sync_audio_stream != nullptr && num_synced_streams == 1) {
      video_stream->SetSyncChannel(config_.voice_engine,
                                   sync_audio_stream->config().voe_channel_id);
    } else {
      video_stream->SetSyncChannel(config_.voice_engine, -1);
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
      if (stream->DeliverRtcp(packet, length)) {
        rtcp_delivered = true;
        if (event_log_)
          event_log_->LogRtcpPacket(true, media_type, packet, length);
      }
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*send_crit_);
    for (VideoSendStream* stream : video_send_streams_) {
      if (stream->DeliverRtcp(packet, length)) {
        rtcp_delivered = true;
        if (event_log_)
          event_log_->LogRtcpPacket(false, media_type, packet, length);
      }
    }
  }
  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(MediaType media_type,
                                                const uint8_t* packet,
                                                size_t length,
                                                const PacketTime& packet_time) {
  // Minimum RTP header size.
  if (length < 12)
    return DELIVERY_PACKET_ERROR;

  uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&packet[8]);

  ReadLockScoped read_lock(*receive_crit_);
  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    auto it = audio_receive_ssrcs_.find(ssrc);
    if (it != audio_receive_ssrcs_.end()) {
      auto status = it->second->DeliverRtp(packet, length, packet_time)
                        ? DELIVERY_OK
                        : DELIVERY_PACKET_ERROR;
      if (status == DELIVERY_OK && event_log_)
        event_log_->LogRtpHeader(true, media_type, packet, length);
      return status;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    auto it = video_receive_ssrcs_.find(ssrc);
    if (it != video_receive_ssrcs_.end()) {
      auto status = it->second->DeliverRtp(packet, length, packet_time)
                        ? DELIVERY_OK
                        : DELIVERY_PACKET_ERROR;
      if (status == DELIVERY_OK && event_log_)
        event_log_->LogRtpHeader(true, media_type, packet, length);
      return status;
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(
    MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const PacketTime& packet_time) {
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(media_type, packet, length);

  return DeliverRtp(media_type, packet, length, packet_time);
}

}  // namespace internal
}  // namespace webrtc
