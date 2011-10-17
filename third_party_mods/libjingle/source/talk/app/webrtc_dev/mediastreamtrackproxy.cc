/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "talk/app/webrtc_dev/mediastreamtrackproxy.h"
#include "talk/app/webrtc_dev/scoped_refptr_msg.h"

namespace {

enum {
  MSG_REGISTER_OBSERVER = 1,
  MSG_UNREGISTER_OBSERVER,
  MSG_LABEL,
  MSG_ENABLED,
  MSG_SET_ENABLED,
  MSG_STATE,
  MSG_SSRC,
  MSG_GET_AUDIODEVICE,
  MSG_GET_VIDEODEVICE,
  MSG_GET_VIDEORENDERER,
  MSG_SET_VIDEORENDERER,
};

typedef talk_base::TypedMessageData<std::string*> LabelMessageData;
typedef talk_base::TypedMessageData<webrtc::Observer*> ObserverMessageData;
typedef talk_base::TypedMessageData
    <webrtc::MediaStreamTrackInterface::TrackState> TrackStateMessageData;
typedef talk_base::TypedMessageData<uint32> SsrcMessageData;
typedef talk_base::TypedMessageData<bool> EnableMessageData;


class AudioDeviceMessageData : public talk_base::MessageData {
 public:
  scoped_refptr<webrtc::AudioDeviceModule> audio_device_;
};

class VideoDeviceMessageData : public talk_base::MessageData {
 public:
  scoped_refptr<webrtc::VideoCaptureModule> video_device_;
};

class VideoRendererMessageData : public talk_base::MessageData {
 public:
  scoped_refptr<webrtc::VideoRendererWrapperInterface> video_renderer_;
};

}  // namespace anonymous

namespace webrtc {

template <class T>
MediaStreamTrackProxy<T>::MediaStreamTrackProxy(
    talk_base::Thread* signaling_thread)
    : signaling_thread_(signaling_thread) {
}

template <class T>
void MediaStreamTrackProxy<T>::Init(MediaStreamTrackInterface* track) {
  track_ = track;
}

template <class T>
const char* MediaStreamTrackProxy<T>::kind() const {
  return track_->kind();
}

template <class T>
std::string MediaStreamTrackProxy<T>::label() const {
  if (!signaling_thread_->IsCurrent()) {
    std::string label;
    LabelMessageData msg(&label);
    Send(MSG_LABEL, &msg);
    return label;
  }
  return track_->label();
}

template <class T>
uint32 MediaStreamTrackProxy<T>::ssrc() const {
  if (!signaling_thread_->IsCurrent()) {
    SsrcMessageData msg(0);
    Send(MSG_SSRC, &msg);
    return msg.data();
  }
  return track_->ssrc();
}

template <class T>
MediaStreamTrackInterface::TrackState MediaStreamTrackProxy<T>::state() const {
  if (!signaling_thread_->IsCurrent()) {
    TrackStateMessageData msg(MediaStreamTrackInterface::kInitializing);
    Send(MSG_STATE, &msg);
    return msg.data();
  }
  return track_->state();
}

template <class T>
bool MediaStreamTrackProxy<T>::enabled() const {
  if (!signaling_thread_->IsCurrent()) {
    EnableMessageData msg(false);
    Send(MSG_ENABLED, &msg);
    return msg.data();
  }
  return track_->enabled();
}

template <class T>
bool MediaStreamTrackProxy<T>::set_enabled(bool enable) {
  if (!signaling_thread_->IsCurrent()) {
    EnableMessageData msg(enable);
    Send(MSG_SET_ENABLED, &msg);
    return msg.data();
  }
  return track_->set_enabled(enable);
}

template <class T>
bool MediaStreamTrackProxy<T>::set_state(
    MediaStreamTrackInterface::TrackState new_state) {
  if (!signaling_thread_->IsCurrent()) {
    // State should only be allowed to be changed from the signaling thread.
    ASSERT(!"Not Allowed!");
    return false;
  }
  return track_->set_state(new_state);
}

template <class T>
bool MediaStreamTrackProxy<T>::set_ssrc(uint32 ssrc) {
  if (!signaling_thread_->IsCurrent()) {
    // ssrc should only be allowed to be changed from the signaling thread.
    ASSERT(!"Not Allowed!");
    return false;
  }
  return track_->set_ssrc(ssrc);
}

template <class T>
void MediaStreamTrackProxy<T>::RegisterObserver(Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_REGISTER_OBSERVER, &msg);
    return;
  }
  track_->RegisterObserver(observer);
}

template <class T>
void MediaStreamTrackProxy<T>::UnregisterObserver(Observer* observer) {
  if (!signaling_thread_->IsCurrent()) {
    ObserverMessageData msg(observer);
    Send(MSG_UNREGISTER_OBSERVER, &msg);
    return;
  }
  track_->UnregisterObserver(observer);
}

template <class T>
void MediaStreamTrackProxy<T>::Send(uint32 id,
                                    talk_base::MessageData* data) const {
  signaling_thread_->Send(const_cast<MediaStreamTrackProxy<T>*>(this), id,
                          data);
}

template <class T>
bool MediaStreamTrackProxy<T>::HandleMessage(talk_base::Message* msg) {
  talk_base::MessageData* data = msg->pdata;
  switch (msg->message_id) {
    case MSG_REGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      track_->RegisterObserver(observer->data());
      return true;
      break;
    }
    case MSG_UNREGISTER_OBSERVER: {
      ObserverMessageData* observer = static_cast<ObserverMessageData*>(data);
      track_->UnregisterObserver(observer->data());
      return true;
      break;
    }
    case MSG_LABEL: {
      LabelMessageData* label = static_cast<LabelMessageData*>(data);
      *(label->data()) = track_->label();
      return true;
    }
    case MSG_SSRC: {
      SsrcMessageData* ssrc = static_cast<SsrcMessageData*>(data);
      ssrc->data() = track_->ssrc();
      return true;
      break;
    }
    case MSG_SET_ENABLED: {
      EnableMessageData* enabled = static_cast<EnableMessageData*>(data);
      enabled->data() = track_->set_enabled(enabled->data());
      return true;
      break;
    }
    case MSG_ENABLED: {
      EnableMessageData* enabled = static_cast<EnableMessageData*>(data);
      enabled->data() = track_->enabled();
      return true;
      break;
    }
    case MSG_STATE: {
      TrackStateMessageData* state = static_cast<TrackStateMessageData*>(data);
      state->data() = track_->state();
      return true;
      break;
    }
    default:
      return false;
  }
}

AudioTrackProxy::AudioTrackProxy(
    const std::string& label,
    uint32 ssrc,
    talk_base::Thread* signaling_thread)
    : MediaStreamTrackProxy<LocalAudioTrackInterface>(signaling_thread),
      audio_track_(AudioTrack::CreateRemote(label, ssrc)) {
  Init(audio_track_);
}

AudioTrackProxy::AudioTrackProxy(
    const std::string& label,
    AudioDeviceModule* audio_device,
    talk_base::Thread* signaling_thread)
    : MediaStreamTrackProxy<LocalAudioTrackInterface>(signaling_thread),
      audio_track_(AudioTrack::CreateLocal(label, audio_device)) {
  Init(audio_track_);
}

scoped_refptr<AudioTrackInterface> AudioTrackProxy::CreateRemote(
    const std::string& label,
    uint32 ssrc,
    talk_base::Thread* signaling_thread) {
  ASSERT(signaling_thread);
  talk_base::RefCountImpl<AudioTrackProxy>* track =
      new talk_base::RefCountImpl<AudioTrackProxy>(label, ssrc,
                                                   signaling_thread);
  return track;
}

scoped_refptr<LocalAudioTrackInterface> AudioTrackProxy::CreateLocal(
    const std::string& label,
    AudioDeviceModule* audio_device,
    talk_base::Thread* signaling_thread) {
  ASSERT(signaling_thread);
  talk_base::RefCountImpl<AudioTrackProxy>* track =
      new talk_base::RefCountImpl<AudioTrackProxy>(label,
                                                   audio_device,
                                                   signaling_thread);
  return track;
}

AudioDeviceModule* AudioTrackProxy::GetAudioDevice() {
  if (!signaling_thread_->IsCurrent()) {
    AudioDeviceMessageData msg;
    Send(MSG_GET_AUDIODEVICE, &msg);
    return msg.audio_device_;
  }
  return audio_track_->GetAudioDevice();
}

void AudioTrackProxy::OnMessage(talk_base::Message* msg) {
  if (!MediaStreamTrackProxy<LocalAudioTrackInterface>::HandleMessage(msg)) {
    if (msg->message_id == MSG_GET_AUDIODEVICE) {
      AudioDeviceMessageData* audio_device =
          static_cast<AudioDeviceMessageData*>(msg->pdata);
      audio_device->audio_device_ = audio_track_->GetAudioDevice();
    } else {
      ASSERT(!"Not Implemented!");
    }
  }
}

VideoTrackProxy::VideoTrackProxy(
    const std::string& label,
    uint32 ssrc,
    talk_base::Thread* signaling_thread)
    : MediaStreamTrackProxy<LocalVideoTrackInterface>(signaling_thread),
      video_track_(VideoTrack::CreateRemote(label, ssrc)) {
  Init(video_track_);
}

VideoTrackProxy::VideoTrackProxy(
    const std::string& label,
    VideoCaptureModule* video_device,
    talk_base::Thread* signaling_thread)
    : MediaStreamTrackProxy<LocalVideoTrackInterface>(signaling_thread),
      video_track_(VideoTrack::CreateLocal(label, video_device)) {
  Init(video_track_);
}

scoped_refptr<VideoTrackInterface> VideoTrackProxy::CreateRemote(
    const std::string& label,
    uint32 ssrc,
    talk_base::Thread* signaling_thread) {
  ASSERT(signaling_thread);
  talk_base::RefCountImpl<VideoTrackProxy>* track =
      new talk_base::RefCountImpl<VideoTrackProxy>(label, ssrc,
                                                   signaling_thread);
  return track;
}

scoped_refptr<LocalVideoTrackInterface> VideoTrackProxy::CreateLocal(
    const std::string& label,
    VideoCaptureModule* video_device,
    talk_base::Thread* signaling_thread) {
  ASSERT(signaling_thread);
  talk_base::RefCountImpl<VideoTrackProxy>* track =
      new talk_base::RefCountImpl<VideoTrackProxy>(label, video_device,
                                                   signaling_thread);
  return track;
}

VideoCaptureModule* VideoTrackProxy::GetVideoCapture() {
  if (!signaling_thread_->IsCurrent()) {
    VideoDeviceMessageData msg;
    Send(MSG_GET_VIDEODEVICE, &msg);
    return msg.video_device_;
  }
  return video_track_->GetVideoCapture();
}

void VideoTrackProxy::SetRenderer(VideoRendererWrapperInterface* renderer) {
  if (!signaling_thread_->IsCurrent()) {
    VideoRendererMessageData msg;
    msg.video_renderer_ = renderer;
    Send(MSG_SET_VIDEORENDERER, &msg);
    return;
  }
  return video_track_->SetRenderer(renderer);
}

VideoRendererWrapperInterface* VideoTrackProxy::GetRenderer() {
  if (!signaling_thread_->IsCurrent()) {
    VideoRendererMessageData msg;
    Send(MSG_GET_VIDEORENDERER, &msg);
    return msg.video_renderer_;
  }
  return video_track_->GetRenderer();
}

void VideoTrackProxy::OnMessage(talk_base::Message* msg) {
  if (!MediaStreamTrackProxy<LocalVideoTrackInterface>::HandleMessage(msg)) {
    switch (msg->message_id) {
      case  MSG_GET_VIDEODEVICE: {
        VideoDeviceMessageData* video_device =
            static_cast<VideoDeviceMessageData*>(msg->pdata);
        video_device->video_device_ = video_track_->GetVideoCapture();
        break;
      }
      case MSG_GET_VIDEORENDERER: {
        VideoRendererMessageData* video_renderer =
            static_cast<VideoRendererMessageData*>(msg->pdata);
        video_renderer->video_renderer_ = video_track_->GetRenderer();
        break;
      }
      case MSG_SET_VIDEORENDERER: {
        VideoRendererMessageData* video_renderer =
            static_cast<VideoRendererMessageData*>(msg->pdata);
        video_track_->SetRenderer(video_renderer->video_renderer_.get());
        break;
      }
    default:
      ASSERT(!"Not Implemented!");
      break;
    }
  }
}

}  // namespace webrtc
