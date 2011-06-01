// Copyright 2011 Google Inc. All Rights Reserved.
// Author: mallinath@google.com (Mallinath Bareddy)

#include "talk/app/webrtcchannelmanager.h"

namespace webrtc {

struct VideoCaptureDeviceParams : public talk_base::MessageData {
  VideoCaptureDeviceParams(const std::string& cam_device)
      : cam_device(cam_device),
        result(false) {}
  const std::string cam_device;
  bool result;
};

struct RenderParams : public talk_base::MessageData {
  RenderParams(int channel_id,
               void* window,
               unsigned int zOrder,
               float left,
               float top,
               float right,
               float bottom)
  :channel_id(channel_id)
  ,window(window)
  ,zOrder(zOrder)
  ,left(left)
  ,top(top)
  ,right(right)
  ,bottom(bottom) {}

  int channel_id;
  void* window;
  unsigned int zOrder;
  float left;
  float top;
  float right;
  float bottom;
  bool result;
};

bool WebRtcChannelManager::Init() {
  return MaybeInit();
}

cricket::VoiceChannel* WebRtcChannelManager::CreateVoiceChannel(
    cricket::BaseSession* s, const std::string& content_name, bool rtcp) {
  return (MaybeInit()) ?
      ChannelManager::CreateVoiceChannel(s, content_name, rtcp) : NULL;
}

cricket::VideoChannel* WebRtcChannelManager::CreateVideoChannel(
    cricket::BaseSession* s, const std::string& content_name, bool rtcp,
    cricket::VoiceChannel* vc) {
  return (MaybeInit()) ?
      ChannelManager::CreateVideoChannel(s, content_name, rtcp, vc) : NULL;

}

cricket::Soundclip* WebRtcChannelManager::CreateSoundclip() {
  return (MaybeInit()) ? ChannelManager::CreateSoundclip() : NULL;
}
void WebRtcChannelManager::DestroyVoiceChannel(cricket::VoiceChannel* vc) {
  ChannelManager::DestroyVoiceChannel(vc);
  MaybeTerm();
}
void WebRtcChannelManager::DestroyVideoChannel(cricket::VideoChannel* vc) {
  ChannelManager::DestroyVideoChannel(vc);
  MaybeTerm();
}
void WebRtcChannelManager::DestroySoundclip(cricket::Soundclip* s) {
  ChannelManager::DestroySoundclip(s);
  MaybeTerm();
}

bool WebRtcChannelManager::MaybeInit() {
  bool ret = initialized();
  if (!ret) {
    ret = ChannelManager::Init();
  }
  return ret;
}

void WebRtcChannelManager::MaybeTerm() {
  if (initialized() && !has_channels()) {
    Terminate();
  }
}

bool WebRtcChannelManager::SetVideoRenderer(int channel_id,
                                            void* window,
                                            unsigned int zOrder,
                                            float left,
                                            float top,
                                            float right,
                                            float bottom) {
  if (MaybeInit()) {
    RenderParams params(channel_id, window, zOrder, left, top, right, bottom);
    return cricket::ChannelManager::Send(MSG_SETRTC_VIDEORENDERER, &params);
  } else {
    return false;
  }
}

void WebRtcChannelManager::SetVideoRenderer_w(int channel_id,
                                              void* window,
                                              unsigned int zOrder,
                                              float left,
                                              float top,
                                              float right,
                                              float bottom) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  ASSERT(initialized());
  media_engine()->SetVideoRenderer(channel_id, window, zOrder, left, top, right, bottom);
}

void WebRtcChannelManager::OnMessage(talk_base::Message *message) {
  talk_base::MessageData* data = message->pdata;
  switch(message->message_id) {
    case MSG_SETRTC_VIDEORENDERER: {
      RenderParams* p = static_cast<RenderParams*>(data);
      SetVideoRenderer_w(p->channel_id,
                         p->window,
                         p->zOrder,
                         p->left,
                         p->top,
                         p->right,
                         p->bottom);
      break;
    }
    default: {
      ChannelManager::OnMessage(message);
    }
  }
}

} // namespace webrtc
