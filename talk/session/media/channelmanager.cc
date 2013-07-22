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

#include "talk/base/bind.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/capturemanager.h"
#include "talk/media/base/hybriddataengine.h"
#include "talk/media/base/rtpdataengine.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/devices/devicemanager.h"
#ifdef HAVE_SCTP
#include "talk/media/sctp/sctpdataengine.h"
#endif
#include "talk/session/media/soundclip.h"

namespace cricket {

enum {
  MSG_VIDEOCAPTURESTATE = 1,
};

using talk_base::Bind;

static const int kNotSetOutputVolume = -1;

struct CaptureStateParams : public talk_base::MessageData {
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

#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
ChannelManager::ChannelManager(talk_base::Thread* worker_thread) {
  Construct(MediaEngineFactory::Create(),
            ConstructDataEngine(),
            cricket::DeviceManagerFactory::Create(),
            new CaptureManager(),
            worker_thread);
}
#endif

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               DeviceManagerInterface* dm,
                               CaptureManager* cm,
                               talk_base::Thread* worker_thread) {
  Construct(me, dme, dm, cm, worker_thread);
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               DeviceManagerInterface* dm,
                               talk_base::Thread* worker_thread) {
  Construct(me,
            ConstructDataEngine(),
            dm,
            new CaptureManager(),
            worker_thread);
}

void ChannelManager::Construct(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               DeviceManagerInterface* dm,
                               CaptureManager* cm,
                               talk_base::Thread* worker_thread) {
  media_engine_.reset(me);
  data_media_engine_.reset(dme);
  device_manager_.reset(dm);
  capture_manager_.reset(cm);
  initialized_ = false;
  main_thread_ = talk_base::Thread::Current();
  worker_thread_ = worker_thread;
  audio_in_device_ = DeviceManagerInterface::kDefaultDeviceName;
  audio_out_device_ = DeviceManagerInterface::kDefaultDeviceName;
  audio_options_ = MediaEngineInterface::DEFAULT_AUDIO_OPTIONS;
  audio_delay_offset_ = MediaEngineInterface::kDefaultAudioDelayOffset;
  audio_output_volume_ = kNotSetOutputVolume;
  local_renderer_ = NULL;
  capturing_ = false;
  monitoring_ = false;
  enable_rtx_ = false;

  // Init the device manager immediately, and set up our default video device.
  SignalDevicesChange.repeat(device_manager_->SignalDevicesChange);
  device_manager_->Init();

  // Camera is started asynchronously, request callbacks when startup
  // completes to be able to forward them to the rendering manager.
  media_engine_->SignalVideoCaptureStateChange().connect(
      this, &ChannelManager::OnVideoCaptureStateChange);
  capture_manager_->SignalCapturerStateChange.connect(
      this, &ChannelManager::OnVideoCaptureStateChange);
}

ChannelManager::~ChannelManager() {
  if (initialized_)
    Terminate();
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

int ChannelManager::GetCapabilities() {
  return media_engine_->GetCapabilities() & device_manager_->GetCapabilities();
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
  if (worker_thread_ && worker_thread_->started()) {
    if (media_engine_->Init(worker_thread_)) {
      initialized_ = true;

      // Now that we're initialized, apply any stored preferences. A preferred
      // device might have been unplugged. In this case, we fallback to the
      // default device but keep the user preferences. The preferences are
      // changed only when the Javascript FE changes them.
      const std::string preferred_audio_in_device = audio_in_device_;
      const std::string preferred_audio_out_device = audio_out_device_;
      const std::string preferred_camera_device = camera_device_;
      Device device;
      if (!device_manager_->GetAudioInputDevice(audio_in_device_, &device)) {
        LOG(LS_WARNING) << "The preferred microphone '" << audio_in_device_
                        << "' is unavailable. Fall back to the default.";
        audio_in_device_ = DeviceManagerInterface::kDefaultDeviceName;
      }
      if (!device_manager_->GetAudioOutputDevice(audio_out_device_, &device)) {
        LOG(LS_WARNING) << "The preferred speaker '" << audio_out_device_
                        << "' is unavailable. Fall back to the default.";
        audio_out_device_ = DeviceManagerInterface::kDefaultDeviceName;
      }
      if (!device_manager_->GetVideoCaptureDevice(camera_device_, &device)) {
        if (!camera_device_.empty()) {
          LOG(LS_WARNING) << "The preferred camera '" << camera_device_
                          << "' is unavailable. Fall back to the default.";
        }
        camera_device_ = DeviceManagerInterface::kDefaultDeviceName;
      }

      if (!SetAudioOptions(audio_in_device_, audio_out_device_,
                           audio_options_, audio_delay_offset_)) {
        LOG(LS_WARNING) << "Failed to SetAudioOptions with"
                        << " microphone: " << audio_in_device_
                        << " speaker: " << audio_out_device_
                        << " options: " << audio_options_
                        << " delay: " << audio_delay_offset_;
      }

      // If audio_output_volume_ has been set via SetOutputVolume(), set the
      // audio output volume of the engine.
      if (kNotSetOutputVolume != audio_output_volume_ &&
          !SetOutputVolume(audio_output_volume_)) {
        LOG(LS_WARNING) << "Failed to SetOutputVolume to "
                        << audio_output_volume_;
      }
      if (!SetCaptureDevice(camera_device_) && !camera_device_.empty()) {
        LOG(LS_WARNING) << "Failed to SetCaptureDevice with camera: "
                        << camera_device_;
      }

      // Restore the user preferences.
      audio_in_device_ = preferred_audio_in_device;
      audio_out_device_ = preferred_audio_out_device;
      camera_device_ = preferred_camera_device;

      // Now apply the default video codec that has been set earlier.
      if (default_video_encoder_config_.max_codec.id != 0) {
        SetDefaultVideoEncoderConfig(default_video_encoder_config_);
      }
      // And the local renderer.
      if (local_renderer_) {
        SetLocalRenderer(local_renderer_);
      }
    }
  }
  return initialized_;
}

void ChannelManager::Terminate() {
  ASSERT(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(Bind(&ChannelManager::Terminate_w, this));
  media_engine_->Terminate();
  initialized_ = false;
}

void ChannelManager::Terminate_w() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  // Need to destroy the voice/video channels
  while (!video_channels_.empty()) {
    DestroyVideoChannel_w(video_channels_.back());
  }
  while (!voice_channels_.empty()) {
    DestroyVoiceChannel_w(voice_channels_.back());
  }
  while (!soundclips_.empty()) {
    DestroySoundclip_w(soundclips_.back());
  }
  if (!SetCaptureDevice_w(NULL)) {
    LOG(LS_WARNING) << "failed to delete video capturer";
  }
}

VoiceChannel* ChannelManager::CreateVoiceChannel(
    BaseSession* session, const std::string& content_name, bool rtcp) {
  return worker_thread_->Invoke<VoiceChannel*>(
      Bind(&ChannelManager::CreateVoiceChannel_w, this,
           session, content_name, rtcp));
}

VoiceChannel* ChannelManager::CreateVoiceChannel_w(
    BaseSession* session, const std::string& content_name, bool rtcp) {
  // This is ok to alloc from a thread other than the worker thread
  ASSERT(initialized_);
  VoiceMediaChannel* media_channel = media_engine_->CreateChannel();
  if (media_channel == NULL)
    return NULL;

  VoiceChannel* voice_channel = new VoiceChannel(
      worker_thread_, media_engine_.get(), media_channel,
      session, content_name, rtcp);
  if (!voice_channel->Init()) {
    delete voice_channel;
    return NULL;
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
  VoiceChannels::iterator it = std::find(voice_channels_.begin(),
      voice_channels_.end(), voice_channel);
  ASSERT(it != voice_channels_.end());
  if (it == voice_channels_.end())
    return;

  voice_channels_.erase(it);
  delete voice_channel;
}

VideoChannel* ChannelManager::CreateVideoChannel(
    BaseSession* session, const std::string& content_name, bool rtcp,
    VoiceChannel* voice_channel) {
  return worker_thread_->Invoke<VideoChannel*>(
      Bind(&ChannelManager::CreateVideoChannel_w, this, session,
           content_name, rtcp, voice_channel));
}

VideoChannel* ChannelManager::CreateVideoChannel_w(
    BaseSession* session, const std::string& content_name, bool rtcp,
    VoiceChannel* voice_channel) {
  // This is ok to alloc from a thread other than the worker thread
  ASSERT(initialized_);
  VideoMediaChannel* media_channel =
      // voice_channel can be NULL in case of NullVoiceEngine.
      media_engine_->CreateVideoChannel(voice_channel ?
          voice_channel->media_channel() : NULL);
  if (media_channel == NULL)
    return NULL;

  VideoChannel* video_channel = new VideoChannel(
      worker_thread_, media_engine_.get(), media_channel,
      session, content_name, rtcp, voice_channel);
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
  VideoChannels::iterator it = std::find(video_channels_.begin(),
      video_channels_.end(), video_channel);
  ASSERT(it != video_channels_.end());
  if (it == video_channels_.end())
    return;

  video_channels_.erase(it);
  delete video_channel;
}

DataChannel* ChannelManager::CreateDataChannel(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType channel_type) {
  return worker_thread_->Invoke<DataChannel*>(
      Bind(&ChannelManager::CreateDataChannel_w, this, session, content_name,
           rtcp, channel_type));
}

DataChannel* ChannelManager::CreateDataChannel_w(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType data_channel_type) {
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
      worker_thread_, media_channel,
      session, content_name, rtcp);
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

Soundclip* ChannelManager::CreateSoundclip() {
  return worker_thread_->Invoke<Soundclip*>(
      Bind(&ChannelManager::CreateSoundclip_w, this));
}

Soundclip* ChannelManager::CreateSoundclip_w() {
  ASSERT(initialized_);
  ASSERT(worker_thread_ == talk_base::Thread::Current());

  SoundclipMedia* soundclip_media = media_engine_->CreateSoundclip();
  if (!soundclip_media) {
    return NULL;
  }

  Soundclip* soundclip = new Soundclip(worker_thread_, soundclip_media);
  soundclips_.push_back(soundclip);
  return soundclip;
}

void ChannelManager::DestroySoundclip(Soundclip* soundclip) {
  if (soundclip) {
    worker_thread_->Invoke<void>(
        Bind(&ChannelManager::DestroySoundclip_w, this, soundclip));
  }
}

void ChannelManager::DestroySoundclip_w(Soundclip* soundclip) {
  // Destroy soundclip.
  ASSERT(initialized_);
  Soundclips::iterator it = std::find(soundclips_.begin(),
      soundclips_.end(), soundclip);
  ASSERT(it != soundclips_.end());
  if (it == soundclips_.end())
    return;

  soundclips_.erase(it);
  delete soundclip;
}

bool ChannelManager::GetAudioOptions(std::string* in_name,
                                     std::string* out_name, int* opts) {
  if (in_name)
    *in_name = audio_in_device_;
  if (out_name)
    *out_name = audio_out_device_;
  if (opts)
    *opts = audio_options_;
  return true;
}

bool ChannelManager::SetAudioOptions(const std::string& in_name,
                                     const std::string& out_name, int opts) {
  return SetAudioOptions(in_name, out_name, opts, audio_delay_offset_);
}

bool ChannelManager::SetAudioOptions(const std::string& in_name,
                                     const std::string& out_name, int opts,
                                     int delay_offset) {
  // Get device ids from DeviceManager.
  Device in_dev, out_dev;
  if (!device_manager_->GetAudioInputDevice(in_name, &in_dev)) {
    LOG(LS_WARNING) << "Failed to GetAudioInputDevice: " << in_name;
    return false;
  }
  if (!device_manager_->GetAudioOutputDevice(out_name, &out_dev)) {
    LOG(LS_WARNING) << "Failed to GetAudioOutputDevice: " << out_name;
    return false;
  }

  // If we're initialized, pass the settings to the media engine.
  bool ret = true;
  if (initialized_) {
    ret = worker_thread_->Invoke<bool>(
        Bind(&ChannelManager::SetAudioOptions_w, this,
             opts, delay_offset, &in_dev, &out_dev));
  }

  // If all worked well, save the values for use in GetAudioOptions.
  if (ret) {
    audio_options_ = opts;
    audio_in_device_ = in_name;
    audio_out_device_ = out_name;
    audio_delay_offset_ = delay_offset;
  }
  return ret;
}

bool ChannelManager::SetAudioOptions_w(int opts, int delay_offset,
    const Device* in_dev, const Device* out_dev) {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  ASSERT(initialized_);

  // Set audio options
  bool ret = media_engine_->SetAudioOptions(opts);

  if (ret) {
    ret = media_engine_->SetAudioDelayOffset(delay_offset);
  }

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

bool ChannelManager::IsSameCapturer(const std::string& capturer_name,
                                    VideoCapturer* capturer) {
  if (capturer == NULL) {
    return false;
  }
  Device device;
  if (!device_manager_->GetVideoCaptureDevice(capturer_name, &device)) {
    return false;
  }
  return capturer->GetId() == device.id;
}

bool ChannelManager::GetVideoCaptureDevice(Device* device) {
  std::string device_name;
  if (!GetCaptureDevice(&device_name)) {
    return false;
  }
  return device_manager_->GetVideoCaptureDevice(device_name, device);
}

bool ChannelManager::GetCaptureDevice(std::string* cam_name) {
  if (camera_device_.empty()) {
    // Initialize camera_device_ with default.
    Device device;
    if (!device_manager_->GetVideoCaptureDevice(
        DeviceManagerInterface::kDefaultDeviceName, &device)) {
      LOG(LS_WARNING) << "Device manager can't find default camera: " <<
          DeviceManagerInterface::kDefaultDeviceName;
      return false;
    }
    camera_device_ = device.name;
  }
  *cam_name = camera_device_;
  return true;
}

bool ChannelManager::SetCaptureDevice(const std::string& cam_name) {
  Device device;
  bool ret = true;
  if (!device_manager_->GetVideoCaptureDevice(cam_name, &device)) {
    if (!cam_name.empty()) {
      LOG(LS_WARNING) << "Device manager can't find camera: " << cam_name;
    }
    ret = false;
  }

  // If we're running, tell the media engine about it.
  if (initialized_ && ret) {
    ret = worker_thread_->Invoke<bool>(
        Bind(&ChannelManager::SetCaptureDevice_w, this, &device));
  }

  // If everything worked, retain the name of the selected camera.
  if (ret) {
    camera_device_ = device.name;
  } else if (camera_device_.empty()) {
    // When video option setting fails, we still want camera_device_ to be in a
    // good state, so we initialize it with default if it's empty.
    Device default_device;
    if (!device_manager_->GetVideoCaptureDevice(
        DeviceManagerInterface::kDefaultDeviceName, &default_device)) {
      LOG(LS_WARNING) << "Device manager can't find default camera: " <<
          DeviceManagerInterface::kDefaultDeviceName;
    }
    camera_device_ = default_device.name;
  }

  return ret;
}

VideoCapturer* ChannelManager::CreateVideoCapturer() {
  Device device;
  if (!device_manager_->GetVideoCaptureDevice(camera_device_, &device)) {
    if (!camera_device_.empty()) {
      LOG(LS_WARNING) << "Device manager can't find camera: " << camera_device_;
    }
    return NULL;
  }
  return device_manager_->CreateVideoCapturer(device);
}

bool ChannelManager::SetCaptureDevice_w(const Device* cam_device) {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  ASSERT(initialized_);

  if (!cam_device) {
    video_device_name_.clear();
    return true;
  }
  video_device_name_ = cam_device->name;
  return true;
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

bool ChannelManager::SetLocalMonitor(bool enable) {
  bool ret = initialized_ && worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::SetLocalMonitor,
           media_engine_.get(), enable));
  if (ret) {
    monitoring_ = enable;
  }
  return ret;
}

bool ChannelManager::SetLocalRenderer(VideoRenderer* renderer) {
  bool ret = true;
  if (initialized_) {
    ret = worker_thread_->Invoke<bool>(
        Bind(&MediaEngineInterface::SetLocalRenderer,
             media_engine_.get(), renderer));
  }
  if (ret) {
    local_renderer_ = renderer;
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

// TODO(janahan): For now pass this request through the mediaengine to the
// voice and video engines to do the real work. Once the capturer refactoring
// is done, we will access the capturer using the ssrc (similar to how the
// renderer is accessed today) and register with it directly.
bool ChannelManager::RegisterVideoProcessor(VideoCapturer* capturer,
                                            VideoProcessor* processor) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&ChannelManager::RegisterVideoProcessor_w, this,
           capturer, processor));
}

bool ChannelManager::RegisterVideoProcessor_w(VideoCapturer* capturer,
                                              VideoProcessor* processor) {
  return capture_manager_->AddVideoProcessor(capturer, processor);
}

bool ChannelManager::UnregisterVideoProcessor(VideoCapturer* capturer,
                                              VideoProcessor* processor) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&ChannelManager::UnregisterVideoProcessor_w, this,
           capturer, processor));
}

bool ChannelManager::UnregisterVideoProcessor_w(VideoCapturer* capturer,
                                                VideoProcessor* processor) {
  return capture_manager_->RemoveVideoProcessor(capturer, processor);
}

bool ChannelManager::RegisterVoiceProcessor(
    uint32 ssrc,
    VoiceProcessor* processor,
    MediaProcessorDirection direction) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::RegisterVoiceProcessor, media_engine_.get(),
           ssrc, processor, direction));
}

bool ChannelManager::UnregisterVoiceProcessor(
    uint32 ssrc,
    VoiceProcessor* processor,
    MediaProcessorDirection direction) {
  return initialized_ && worker_thread_->Invoke<bool>(
      Bind(&MediaEngineInterface::UnregisterVoiceProcessor,
           media_engine_.get(), ssrc, processor, direction));
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

void ChannelManager::OnMessage(talk_base::Message* message) {
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


static void GetDeviceNames(const std::vector<Device>& devs,
                           std::vector<std::string>* names) {
  names->clear();
  for (size_t i = 0; i < devs.size(); ++i) {
    names->push_back(devs[i].name);
  }
}

bool ChannelManager::GetAudioInputDevices(std::vector<std::string>* names) {
  names->clear();
  std::vector<Device> devs;
  bool ret = device_manager_->GetAudioInputDevices(&devs);
  if (ret)
    GetDeviceNames(devs, names);

  return ret;
}

bool ChannelManager::GetAudioOutputDevices(std::vector<std::string>* names) {
  names->clear();
  std::vector<Device> devs;
  bool ret = device_manager_->GetAudioOutputDevices(&devs);
  if (ret)
    GetDeviceNames(devs, names);

  return ret;
}

bool ChannelManager::GetVideoCaptureDevices(std::vector<std::string>* names) {
  names->clear();
  std::vector<Device> devs;
  bool ret = device_manager_->GetVideoCaptureDevices(&devs);
  if (ret)
    GetDeviceNames(devs, names);

  return ret;
}

void ChannelManager::SetVideoCaptureDeviceMaxFormat(
    const std::string& usb_id,
    const VideoFormat& max_format) {
  device_manager_->SetVideoCaptureDeviceMaxFormat(usb_id, max_format);
}

VideoFormat ChannelManager::GetStartCaptureFormat() {
  return worker_thread_->Invoke<VideoFormat>(
      Bind(&MediaEngineInterface::GetStartCaptureFormat, media_engine_.get()));
}

}  // namespace cricket
