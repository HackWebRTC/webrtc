// Copyright 2011 Google Inc. All Rights Reserved.
// Author: mallinath@google.com (Mallinath Bareddy)


#ifndef TALK_APP_WEBRTC_WEBRTCCHANNELMANAGER_H_
#define TALK_APP_WEBRTC_WEBRTCCHANNELMANAGER_H_

#include "talk/session/phone/channelmanager.h"

namespace webrtc {

class AudioDeviceModule;

enum {
  MSG_SETRTC_VIDEORENDERER = 21, // Set internal video renderer
};

// WebRtcChannelManager automatically takes care of initialization and
// cricket::ChannelManager. Terminates when not needed

class WebRtcChannelManager : public cricket::ChannelManager {
 public:
  WebRtcChannelManager(talk_base::Thread* worker_thread)
      : ChannelManager(worker_thread) {
  }

  WebRtcChannelManager(cricket::MediaEngine* me, cricket::DeviceManager* dm,
                       talk_base::Thread* worker_thread)
      : ChannelManager(me, dm, worker_thread) {
  }

  bool Init();
  cricket::VoiceChannel* CreateVoiceChannel(
      cricket::BaseSession* s, const std::string& content_name, bool rtcp);
  cricket::VideoChannel* CreateVideoChannel(
      cricket::BaseSession* s, const std::string& content_name, bool rtcp,
      cricket::VoiceChannel* vc);
  cricket::Soundclip* CreateSoundclip();
  void DestroyVoiceChannel(cricket::VoiceChannel* vc);
  void DestroyVideoChannel(cricket::VideoChannel* vc);
  void DestroySoundclip(cricket::Soundclip* s);

  bool SetVideoRenderer(int channel_id,
                        void* window,
                        unsigned int zOrder,
                        float left,
                        float top,
                        float right,
                        float bottom);

 private:
  bool MaybeInit();
  void MaybeTerm();
  void SetExternalAdm_w(AudioDeviceModule* external_adm);
  void SetVideoRenderer_w(int channel_id,
                          void* window,
                          unsigned int zOrder,
                          float left,
                          float top,
                          float right,
                          float bottom);
  void OnMessage(talk_base::Message *message);
};

} // namespace webrtc


#endif /* TALK_APP_WEBRTC_WEBRTCCHANNELMANAGER_H_ */
