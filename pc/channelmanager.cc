/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/channelmanager.h"

#include <algorithm>

#include "media/base/device.h"
#include "media/base/rtpdataengine.h"
#include "pc/srtpfilter.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/trace_event.h"

namespace cricket {


using rtc::Bind;

ChannelManager::ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                               std::unique_ptr<DataEngineInterface> dme,
                               rtc::Thread* thread) {
  Construct(std::move(me), std::move(dme), thread, thread);
}

ChannelManager::ChannelManager(std::unique_ptr<MediaEngineInterface> me,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  Construct(std::move(me),
            std::unique_ptr<DataEngineInterface>(new RtpDataEngine()),
            worker_thread, network_thread);
}

void ChannelManager::Construct(std::unique_ptr<MediaEngineInterface> me,
                               std::unique_ptr<DataEngineInterface> dme,
                               rtc::Thread* worker_thread,
                               rtc::Thread* network_thread) {
  media_engine_ = std::move(me);
  data_media_engine_ = std::move(dme);
  initialized_ = false;
  main_thread_ = rtc::Thread::Current();
  worker_thread_ = worker_thread;
  network_thread_ = network_thread;
  capturing_ = false;
  enable_rtx_ = false;
}

ChannelManager::~ChannelManager() {
  if (initialized_) {
    Terminate();
    // If srtp is initialized (done by the Channel) then we must call
    // srtp_shutdown to free all crypto kernel lists. But we need to make sure
    // shutdown always called at the end, after channels are destroyed.
    // ChannelManager d'tor is always called last, it's safe place to call
    // shutdown.
    ShutdownSrtp();
  }
  // The media engine needs to be deleted on the worker thread for thread safe
  // destruction,
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE, Bind(&ChannelManager::DestructorDeletes_w, this));
}

bool ChannelManager::SetVideoRtxEnabled(bool enable) {
  // To be safe, this call is only allowed before initialization. Apps like
  // Flute only have a singleton ChannelManager and we don't want this flag to
  // be toggled between calls or when there's concurrent calls. We expect apps
  // to enable this at startup and retain that setting for the lifetime of the
  // app.
  if (!initialized_) {
    enable_rtx_ = enable;
    return true;
  } else {
    LOG(LS_WARNING) << "Cannot toggle rtx after initialization!";
    return false;
  }
}

void ChannelManager::GetSupportedAudioSendCodecs(
    std::vector<AudioCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  *codecs = media_engine_->audio_send_codecs();
}

void ChannelManager::GetSupportedAudioReceiveCodecs(
    std::vector<AudioCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  *codecs = media_engine_->audio_recv_codecs();
}

void ChannelManager::GetSupportedAudioRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  if (!media_engine_) {
    return;
  }
  *ext = media_engine_->GetAudioCapabilities().header_extensions;
}

void ChannelManager::GetSupportedVideoCodecs(
    std::vector<VideoCodec>* codecs) const {
  if (!media_engine_) {
    return;
  }
  codecs->clear();

  std::vector<VideoCodec> video_codecs = media_engine_->video_codecs();
  for (const auto& video_codec : video_codecs) {
    if (!enable_rtx_ &&
        _stricmp(kRtxCodecName, video_codec.name.c_str()) == 0) {
      continue;
    }
    codecs->push_back(video_codec);
  }
}

void ChannelManager::GetSupportedVideoRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  if (!media_engine_) {
    return;
  }
  *ext = media_engine_->GetVideoCapabilities().header_extensions;
}

void ChannelManager::GetSupportedDataCodecs(
    std::vector<DataCodec>* codecs) const {
  if (!data_media_engine_) {
    return;
  }
  *codecs = data_media_engine_->data_codecs();
}

bool ChannelManager::Init() {
  RTC_DCHECK(!initialized_);
  if (initialized_) {
    return false;
  }
  RTC_DCHECK(network_thread_);
  RTC_DCHECK(worker_thread_);
  if (!network_thread_->IsCurrent()) {
    // Do not allow invoking calls to other threads on the network thread.
    network_thread_->Invoke<bool>(
        RTC_FROM_HERE,
        rtc::Bind(&rtc::Thread::SetAllowBlockingCalls, network_thread_, false));
  }

  initialized_ = worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&ChannelManager::InitMediaEngine_w, this));
  RTC_DCHECK(initialized_);
  return initialized_;
}

bool ChannelManager::InitMediaEngine_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  if (media_engine_) {
    return media_engine_->Init();
  }
  return true;
}

void ChannelManager::Terminate() {
  RTC_DCHECK(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(RTC_FROM_HERE,
                               Bind(&ChannelManager::Terminate_w, this));
  initialized_ = false;
}

void ChannelManager::DestructorDeletes_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  media_engine_.reset(nullptr);
}

void ChannelManager::Terminate_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  // Need to destroy the voice/video channels
  video_channels_.clear();
  voice_channels_.clear();
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVoiceChannel_w, this, call, media_config,
           rtp_transport, rtcp_transport, rtp_transport, rtcp_transport,
           signaling_thread, content_name, srtp_required, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    rtc::PacketTransportInternal* rtp_transport,
    rtc::PacketTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVoiceChannel_w, this, call, media_config,
           nullptr, nullptr, rtp_transport, rtcp_transport, signaling_thread,
           content_name, srtp_required, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel_w(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const AudioOptions& options) {
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  RTC_DCHECK(nullptr != call);
  if (!media_engine_) {
    return nullptr;
  }

  VoiceMediaChannel* media_channel = media_engine_->CreateChannel(
      call, media_config, options);
  if (!media_channel) {
    return nullptr;
  }

  std::unique_ptr<VoiceChannel> voice_channel(
      new VoiceChannel(worker_thread_, network_thread_, signaling_thread,
                       media_engine_.get(), media_channel, content_name,
                       rtcp_packet_transport == nullptr, srtp_required));

  if (!voice_channel->Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                             rtp_packet_transport, rtcp_packet_transport)) {
    return nullptr;
  }
  VoiceChannel* voice_channel_ptr = voice_channel.get();
  voice_channels_.push_back(std::move(voice_channel));
  return voice_channel_ptr;
}

void ChannelManager::DestroyVoiceChannel(VoiceChannel* voice_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVoiceChannel");
  if (voice_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyVoiceChannel_w, this, voice_channel));
  }
}

void ChannelManager::DestroyVoiceChannel_w(VoiceChannel* voice_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVoiceChannel_w");
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());

  auto it = std::find_if(voice_channels_.begin(), voice_channels_.end(),
                         [&](const std::unique_ptr<VoiceChannel>& p) {
                           return p.get() == voice_channel;
                         });
  RTC_DCHECK(it != voice_channels_.end());
  if (it == voice_channels_.end())
    return;
  voice_channels_.erase(it);
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVideoChannel_w, this, call, media_config,
           rtp_transport, rtcp_transport, rtp_transport, rtcp_transport,
           signaling_thread, content_name, srtp_required, options));
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    rtc::PacketTransportInternal* rtp_transport,
    rtc::PacketTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      RTC_FROM_HERE,
      Bind(&ChannelManager::CreateVideoChannel_w, this, call, media_config,
           nullptr, nullptr, rtp_transport, rtcp_transport, signaling_thread,
           content_name, srtp_required, options));
}

VideoChannel* ChannelManager::CreateVideoChannel_w(
    webrtc::Call* call,
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required,
    const VideoOptions& options) {
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  RTC_DCHECK(nullptr != call);
  VideoMediaChannel* media_channel = media_engine_->CreateVideoChannel(
      call, media_config, options);
  if (!media_channel) {
    return nullptr;
  }

  std::unique_ptr<VideoChannel> video_channel(new VideoChannel(
      worker_thread_, network_thread_, signaling_thread, media_channel,
      content_name, rtcp_packet_transport == nullptr, srtp_required));
  if (!video_channel->Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                             rtp_packet_transport, rtcp_packet_transport)) {
    return nullptr;
  }
  VideoChannel* video_channel_ptr = video_channel.get();
  video_channels_.push_back(std::move(video_channel));
  return video_channel_ptr;
}

void ChannelManager::DestroyVideoChannel(VideoChannel* video_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVideoChannel");
  if (video_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyVideoChannel_w, this, video_channel));
  }
}

void ChannelManager::DestroyVideoChannel_w(VideoChannel* video_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyVideoChannel_w");
  RTC_DCHECK(initialized_);
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());

  auto it = std::find_if(video_channels_.begin(), video_channels_.end(),
                         [&](const std::unique_ptr<VideoChannel>& p) {
                           return p.get() == video_channel;
                         });
  RTC_DCHECK(it != video_channels_.end());
  if (it == video_channels_.end())
    return;

  video_channels_.erase(it);
}

RtpDataChannel* ChannelManager::CreateRtpDataChannel(
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required) {
  return worker_thread_->Invoke<RtpDataChannel*>(
      RTC_FROM_HERE, Bind(&ChannelManager::CreateRtpDataChannel_w, this,
                          media_config, rtp_transport, rtcp_transport,
                          signaling_thread, content_name, srtp_required));
}

RtpDataChannel* ChannelManager::CreateRtpDataChannel_w(
    const cricket::MediaConfig& media_config,
    DtlsTransportInternal* rtp_transport,
    DtlsTransportInternal* rtcp_transport,
    rtc::Thread* signaling_thread,
    const std::string& content_name,
    bool srtp_required) {
  // This is ok to alloc from a thread other than the worker thread.
  RTC_DCHECK(initialized_);
  DataMediaChannel* media_channel
      = data_media_engine_->CreateChannel(media_config);
  if (!media_channel) {
    LOG(LS_WARNING) << "Failed to create RTP data channel.";
    return nullptr;
  }

  std::unique_ptr<RtpDataChannel> data_channel(new RtpDataChannel(
      worker_thread_, network_thread_, signaling_thread, media_channel,
      content_name, rtcp_transport == nullptr, srtp_required));
  if (!data_channel->Init_w(rtp_transport, rtcp_transport, rtp_transport,
                            rtcp_transport)) {
    LOG(LS_WARNING) << "Failed to init data channel.";
    return nullptr;
  }
  RtpDataChannel* data_channel_ptr = data_channel.get();
  data_channels_.push_back(std::move(data_channel));
  return data_channel_ptr;
}

void ChannelManager::DestroyRtpDataChannel(RtpDataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyRtpDataChannel");
  if (data_channel) {
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE,
        Bind(&ChannelManager::DestroyRtpDataChannel_w, this, data_channel));
  }
}

void ChannelManager::DestroyRtpDataChannel_w(RtpDataChannel* data_channel) {
  TRACE_EVENT0("webrtc", "ChannelManager::DestroyRtpDataChannel_w");
  RTC_DCHECK(initialized_);

  auto it = std::find_if(data_channels_.begin(), data_channels_.end(),
                         [&](const std::unique_ptr<RtpDataChannel>& p) {
                           return p.get() == data_channel;
                         });
  RTC_DCHECK(it != data_channels_.end());
  if (it == data_channels_.end())
    return;

  data_channels_.erase(it);
}

bool ChannelManager::StartAecDump(rtc::PlatformFile file,
                                  int64_t max_size_bytes) {
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&MediaEngineInterface::StartAecDump,
                          media_engine_.get(), file, max_size_bytes));
}

void ChannelManager::StopAecDump() {
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(&MediaEngineInterface::StopAecDump, media_engine_.get()));
}

}  // namespace cricket
