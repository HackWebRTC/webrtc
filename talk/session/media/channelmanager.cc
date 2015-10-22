/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/session/media/channelmanager.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>

#include "talk/app/webrtc/mediacontroller.h"
#include "talk/media/base/capturemanager.h"
#include "talk/media/base/device.h"
#include "talk/media/base/hybriddataengine.h"
#include "talk/media/base/rtpdataengine.h"
#include "talk/media/base/videocapturer.h"
#ifdef HAVE_SCTP
#include "talk/media/sctp/sctpdataengine.h"
#endif
#include "talk/session/media/srtpfilter.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace cricket {

enum {
  MSG_VIDEOCAPTURESTATE = 1,
};

using rtc::Bind;

static const int kNotSetOutputVolume = -1;

struct CaptureStateParams : public rtc::MessageData {
  CaptureStateParams(cricket::VideoCapturer* c, cricket::CaptureState s)
      : capturer(c),
        state(s) {}
  cricket::VideoCapturer* capturer;
  cricket::CaptureState state;
};

static DataEngineInterface* ConstructDataEngine() {
#ifdef HAVE_SCTP
  return new HybridDataEngine(new RtpDataEngine(), new SctpDataEngine());
#else
  return new RtpDataEngine();
#endif
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               CaptureManager* cm,
                               rtc::Thread* worker_thread) {
  Construct(me, dme, cm, worker_thread);
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               rtc::Thread* worker_thread) {
  Construct(me,
            ConstructDataEngine(),
            new CaptureManager(),
            worker_thread);
}

void ChannelManager::Construct(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               CaptureManager* cm,
                               rtc::Thread* worker_thread) {
  media_engine_.reset(me);
  data_media_engine_.reset(dme);
  capture_manager_.reset(cm);
  initialized_ = false;
  main_thread_ = rtc::Thread::Current();
  worker_thread_ = worker_thread;
  // Get the default audio options from the media engine.
  audio_options_ = media_engine_->GetAudioOptions();
  audio_output_volume_ = kNotSetOutputVolume;
  local_renderer_ = NULL;
  capturing_ = false;
  enable_rtx_ = false;

  capture_manager_->SignalCapturerStateChange.connect(
      this, &ChannelManager::OnVideoCaptureStateChange);
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
  // Some deletes need to be on the worker thread for thread safe destruction,
  // this includes the media engine and capture manager.
  worker_thread_->Invoke<void>(Bind(
      &ChannelManager::DestructorDeletes_w, this));
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

void ChannelManager::GetSupportedAudioCodecs(
    std::vector<AudioCodec>* codecs) const {
  codecs->clear();

  for (std::vector<AudioCodec>::const_iterator it =
           media_engine_->audio_codecs().begin();
      it != media_engine_->audio_codecs().end(); ++it) {
    codecs->push_back(*it);
  }
}

void ChannelManager::GetSupportedAudioRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  *ext = media_engine_->audio_rtp_header_extensions();
}

void ChannelManager::GetSupportedVideoCodecs(
    std::vector<VideoCodec>* codecs) const {
  codecs->clear();

  std::vector<VideoCodec>::const_iterator it;
  for (it = media_engine_->video_codecs().begin();
      it != media_engine_->video_codecs().end(); ++it) {
    if (!enable_rtx_ && _stricmp(kRtxCodecName, it->name.c_str()) == 0) {
      continue;
    }
    codecs->push_back(*it);
  }
}

void ChannelManager::GetSupportedVideoRtpHeaderExtensions(
    RtpHeaderExtensions* ext) const {
  *ext = media_engine_->video_rtp_header_extensions();
}

void ChannelManager::GetSupportedDataCodecs(
    std::vector<DataCodec>* codecs) const {
  *codecs = data_media_engine_->data_codecs();
}

bool ChannelManager::Init() {
  ASSERT(!initialized_);
  if (initialized_) {
    return false;
  }
  ASSERT(worker_thread_ != NULL);
  if (!worker_thread_) {
    return false;
  }
  if (worker_thread_ != rtc::Thread::Current()) {
    // Do not allow invoking calls to other threads on the worker thread.
    worker_thread_->Invoke<bool>(rtc::Bind(
        &rtc::Thread::SetAllowBlockingCalls, worker_thread_, false));
  }

  initialized_ = worker_thread_->Invoke<bool>(Bind(
      &ChannelManager::InitMediaEngine_w, this));
  ASSERT(initialized_);
  if (!initialized_) {
    return false;
  }

  if (!SetAudioOptions(audio_options_)) {
    LOG(LS_WARNING) << "Failed to SetAudioOptions with options: "
                    << audio_options_.ToString();
  }

  // If audio_output_volume_ has been set via SetOutputVolume(), set the
  // audio output volume of the engine.
  if (kNotSetOutputVolume != audio_output_volume_ &&
      !SetOutputVolume(audio_output_volume_)) {
    LOG(LS_WARNING) << "Failed to SetOutputVolume to "
                    << audio_output_volume_;
  }

  // Now apply the default video codec that has been set earlier.
  if (default_video_encoder_config_.max_codec.id != 0) {
    SetDefaultVideoEncoderConfig(default_video_encoder_config_);
  }

  return initialized_;
}

bool ChannelManager::InitMediaEngine_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  return (media_engine_->Init(worker_thread_));
}

void ChannelManager::Terminate() {
  ASSERT(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(Bind(&ChannelManager::Terminate_w, this));
  initialized_ = false;
}

void ChannelManager::DestructorDeletes_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  media_engine_.reset(NULL);
  capture_manager_.reset(NULL);
}

void ChannelManager::Terminate_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  // Need to destroy the voice/video channels
  while (!video_channels_.empty()) {
    DestroyVideoChannel_w(video_channels_.back());
  }
  while (!voice_channels_.empty()) {
    DestroyVoiceChannel_w(voice_channels_.back());
  }
  media_engine_->Terminate();
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    const AudioOptions& options) {
  return worker_thread_->Invoke<VoiceChannel*>(
      Bind(&ChannelManager::CreateVoiceChannel_w, this, media_controller,
           transport_controller, content_name, rtcp, options));
}

VoiceChannel* ChannelManager::CreateVoiceChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    const AudioOptions& options) {
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  ASSERT(nullptr != media_controller);
  VoiceMediaChannel* media_channel =
      media_engine_->CreateChannel(media_controller->call_w(), options);
  if (!media_channel)
    return nullptr;

  VoiceChannel* voice_channel =
      new VoiceChannel(worker_thread_, media_engine_.get(), media_channel,
                       transport_controller, content_name, rtcp);
  if (!voice_channel->Init()) {
    delete voice_channel;
    return nullptr;
  }
  voice_channels_.push_back(voice_channel);
  return voice_channel;
}

void ChannelManager::DestroyVoiceChannel(VoiceChannel* voice_channel) {
  if (voice_channel) {
    worker_thread_->Invoke<void>(
        Bind(&ChannelManager::DestroyVoiceChannel_w, this, voice_channel));
  }
}

void ChannelManager::DestroyVoiceChannel_w(VoiceChannel* voice_channel) {
  // Destroy voice channel.
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  VoiceChannels::iterator it = std::find(voice_channels_.begin(),
      voice_channels_.end(), voice_channel);
  ASSERT(it != voice_channels_.end());
  if (it == voice_channels_.end())
    return;
  voice_channels_.erase(it);
  delete voice_channel;
}

VideoChannel* ChannelManager::CreateVideoChannel(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    const VideoOptions& options) {
  return worker_thread_->Invoke<VideoChannel*>(
      Bind(&ChannelManager::CreateVideoChannel_w, this, media_controller,
           transport_controller, content_name, rtcp, options));
}

VideoChannel* ChannelManager::CreateVideoChannel_w(
    webrtc::MediaControllerInterface* media_controller,
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    const VideoOptions& options) {
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  ASSERT(nullptr != media_controller);
  VideoMediaChannel* media_channel =
      media_engine_->CreateVideoChannel(media_controller->call_w(), options);
  if (media_channel == NULL) {
    return NULL;
  }

  VideoChannel* video_channel = new VideoChannel(
      worker_thread_, media_channel, transport_controller, content_name, rtcp);
  if (!video_channel->Init()) {
    delete video_channel;
    return NULL;
  }
  video_channels_.push_back(video_channel);
  return video_channel;
}

void ChannelManager::DestroyVideoChannel(VideoChannel* video_channel) {
  if (video_channel) {
    worker_thread_->Invoke<void>(
        Bind(&ChannelManager::DestroyVideoChannel_w, this, video_channel));
  }
}

void ChannelManager::DestroyVideoChannel_w(VideoChannel* video_channel) {
  // Destroy video channel.
  ASSERT(initialized_);
  ASSERT(worker_thread_ == rtc::Thread::Current());
  VideoChannels::iterator it = std::find(video_channels_.begin(),
      video_channels_.end(), video_channel);
  ASSERT(it != video_channels_.end());
  if (it == video_channels_.end())
    return;

  video_channels_.erase(it);
  delete video_channel;
}

DataChannel* ChannelManager::CreateDataChannel(
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    DataChannelType channel_type) {
  return worker_thread_->Invoke<DataChannel*>(
      Bind(&ChannelManager::CreateDataChannel_w, this, transport_controller,
           content_name, rtcp, channel_type));
}

DataChannel* ChannelManager::CreateDataChannel_w(
    TransportController* transport_controller,
    const std::string& content_name,
    bool rtcp,
    DataChannelType data_channel_type) {
  // This is ok to alloc from a thread other than the worker thread.
  ASSERT(initialized_);
  DataMediaChannel* media_channel = data_media_engine_->CreateChannel(
      data_channel_type);
  if (!media_channel) {
    LOG(LS_WARNING) << "Failed to create data channel of type "
                    << data_channel_type;
    return NULL;
  }

  DataChannel* data_channel = new DataChannel(
      worker_thread_, media_channel, transport_controller, content_name, rtcp);
  if (!data_channel->Init()) {
    LOG(LS_WARNING) << "Failed to init data channel.";
    delete data_channel;
    return NULL;
  }
  data_channels_.push_back(data_channel);
  return data_channel;
}

void ChannelManager::DestroyDataChannel(DataChannel* data_channel) {
  if (data_channel) {
    worker_thread_->Invoke<void>(
        Bind(&ChannelManager::DestroyDataChannel_w, this, data_channel));
  }
}

void ChannelManager::DestroyDataChannel_w(DataChannel* data_channel) {
  // Destroy data channel.
  ASSERT(initialized_);
  DataChannels::iterator it = std::find(data_channels_.begin(),
      data_channels_.end(), data_channel);
  ASSERT(it != data_channels_.end());
  if (it == data_channels_.end())
    return;

  data_channels_.erase(it);
  delete data_channel;
}

bool ChannelManager::SetAudioOptions(const AudioOptions& options) {
  // "Get device ids from DeviceManager" - these are the defaults returned.
  Device in_dev("", -1);
  Device out_dev("", -1);

  // If we're initialized, pass the settings to the media engine.
  bool ret = true;
  if (initialized_) {
    ret = worker_thread_->Invoke<bool>(
        Bind(&ChannelManager::SetAudioOptions_w, this,
             options, &in_dev, &out_dev));
  }

  // If all worked well, save the values for use in GetAudioOptions.
  if (ret) {
    audio_options_ = options;
  }
  return ret;
}

bool ChannelManager::SetAudioOptions_w(
    const AudioOptions& options,
    const Device* in_dev, const Device* out_dev) {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  ASSERT(initialized_);

  // Set audio options
  bool ret = media_engine_->SetAudioOptions(options);

  // Set the audio devices
  if (ret) {
    ret = media_engine_->SetSoundDevices(in_dev, out_dev);
  }

  return ret;
}

bool ChannelManager::GetOutputVolume(int* level) {
  if (!initialized_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::GetOutputVolume, media_engine_.get(), level));
}

bool ChannelManager::SetOutputVolume(int level) {
  bool ret = level >= 0 && level <= 255;
  if (initialized_) {
    ret &= worker_thread_->Invoke<bool>(
        Bind(&MediaEngineInterface::SetOutputVolume,
             media_engine_.get(), level));
  }

  if (ret) {
    audio_output_volume_ = level;
  }

  return ret;
}

bool ChannelManager::SetDefaultVideoEncoderConfig(const VideoEncoderConfig& c) {
  bool ret = true;
  if (initialized_) {
    ret = worker_thread_->Invoke<bool>(
        Bind(&MediaEngineInterface::SetDefaultVideoEncoderConfig,
             media_engine_.get(), c));
  }
  if (ret) {
    default_video_encoder_config_ = c;
  }
  return ret;
}

void ChannelManager::SetVoiceLogging(int level, const char* filter) {
  if (initialized_) {
    worker_thread_->Invoke<void>(
        Bind(&MediaEngineInterface::SetVoiceLogging,
             media_engine_.get(), level, filter));
  } else {
    media_engine_->SetVoiceLogging(level, filter);
  }
}

void ChannelManager::SetVideoLogging(int level, const char* filter) {
  if (initialized_) {
    worker_thread_->Invoke<void>(
        Bind(&MediaEngineInterface::SetVideoLogging,
             media_engine_.get(), level, filter));
  } else {
    media_engine_->SetVideoLogging(level, filter);
  }
}

std::vector<cricket::VideoFormat> ChannelManager::GetSupportedFormats(
    VideoCapturer* capturer) const {
  ASSERT(capturer != NULL);
  std::vector<VideoFormat> formats;
  worker_thread_->Invoke<void>(rtc::Bind(&ChannelManager::GetSupportedFormats_w,
                                         this, capturer, &formats));
  return formats;
}

void ChannelManager::GetSupportedFormats_w(
    VideoCapturer* capturer,
    std::vector<cricket::VideoFormat>* out_formats) const {
  const std::vector<VideoFormat>* formats = capturer->GetSupportedFormats();
  if (formats != NULL)
    *out_formats = *formats;
}

// The following are done in the new "CaptureManager" style that
// all local video capturers, processors, and managers should move
// to.
// TODO(pthatcher): Add more of the CaptureManager interface.
bool ChannelManager::StartVideoCapture(
    VideoCapturer* capturer, const VideoFormat& video_format) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&CaptureManager::StartVideoCapture,
           capture_manager_.get(), capturer, video_format));
}

bool ChannelManager::MuteToBlackThenPause(
    VideoCapturer* video_capturer, bool muted) {
  if (!initialized_) {
    return false;
  }
  worker_thread_->Invoke<void>(
      Bind(&VideoCapturer::MuteToBlackThenPause, video_capturer, muted));
  return true;
}

bool ChannelManager::StopVideoCapture(
    VideoCapturer* capturer, const VideoFormat& video_format) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&CaptureManager::StopVideoCapture,
           capture_manager_.get(), capturer, video_format));
}

bool ChannelManager::RestartVideoCapture(
    VideoCapturer* video_capturer,
    const VideoFormat& previous_format,
    const VideoFormat& desired_format,
    CaptureManager::RestartOptions options) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&CaptureManager::RestartVideoCapture, capture_manager_.get(),
           video_capturer, previous_format, desired_format, options));
}

bool ChannelManager::AddVideoRenderer(
    VideoCapturer* capturer, VideoRenderer* renderer) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&CaptureManager::AddVideoRenderer,
           capture_manager_.get(), capturer, renderer));
}

bool ChannelManager::RemoveVideoRenderer(
    VideoCapturer* capturer, VideoRenderer* renderer) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&CaptureManager::RemoveVideoRenderer,
           capture_manager_.get(), capturer, renderer));
}

bool ChannelManager::IsScreencastRunning() const {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&ChannelManager::IsScreencastRunning_w, this));
}

bool ChannelManager::IsScreencastRunning_w() const {
  VideoChannels::const_iterator it = video_channels_.begin();
  for ( ; it != video_channels_.end(); ++it) {
    if ((*it) && (*it)->IsScreencasting()) {
      return true;
    }
  }
  return false;
}

void ChannelManager::OnVideoCaptureStateChange(VideoCapturer* capturer,
                                               CaptureState result) {
  // TODO(whyuan): Check capturer and signal failure only for camera video, not
  // screencast.
  capturing_ = result == CS_RUNNING;
  main_thread_->Post(this, MSG_VIDEOCAPTURESTATE,
                     new CaptureStateParams(capturer, result));
}

void ChannelManager::OnMessage(rtc::Message* message) {
  switch (message->message_id) {
    case MSG_VIDEOCAPTURESTATE: {
      CaptureStateParams* data =
          static_cast<CaptureStateParams*>(message->pdata);
      SignalVideoCaptureStateChange(data->capturer, data->state);
      delete data;
      break;
    }
  }
}

bool ChannelManager::StartAecDump(rtc::PlatformFile file) {
  return worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::StartAecDump, media_engine_.get(), file));
}

void ChannelManager::StopAecDump() {
  worker_thread_->Invoke<void>(
      Bind(&MediaEngineInterface::StopAecDump, media_engine_.get()));
}

bool ChannelManager::StartRtcEventLog(rtc::PlatformFile file) {
  return worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::StartRtcEventLog, media_engine_.get(), file));
}

void ChannelManager::StopRtcEventLog() {
  worker_thread_->Invoke<void>(
      Bind(&MediaEngineInterface::StopRtcEventLog, media_engine_.get()));
}

}  // namespace cricket
