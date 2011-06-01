/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSIONIMPL_H_
#define TALK_APP_WEBRTC_WEBRTCSESSIONIMPL_H_

#include <string>
#include <vector>

#include "talk/base/messagehandler.h"
#include "talk/p2p/base/candidate.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/mediachannel.h"
#include "talk/app/pc_transport_impl.h"
#include "talk/app/webrtcsession.h"

namespace cricket {
class ChannelManager;
class Transport;
class TransportChannel;
class VoiceChannel;
class VideoChannel;
struct ConnectionInfo;
}

namespace Json {
class Value;
}

namespace webrtc {

struct StreamInfo {
  StreamInfo(const std::string stream_id)
    : channel(NULL),
      transport(NULL),
      video(false),
      stream_id(stream_id),
      media_channel(-1) {}

  StreamInfo()
    : channel(NULL),
      transport(NULL),
      video(false),
      media_channel(-1) {}

  cricket::BaseChannel* channel;
  PC_Transport_Impl* transport; //TODO - add RTCP transport channel
  bool video;
  std::string stream_id;
  int media_channel;
};

typedef std::vector<cricket::AudioCodec> AudioCodecs;
typedef std::vector<cricket::VideoCodec> VideoCodecs;

class ExternalRenderer;
class PeerConnection;
class WebRtcChannelManager;

class WebRTCSessionImpl: public WebRTCSession {

 public:

  WebRTCSessionImpl(const std::string& id,
                    const std::string& direction,
                    cricket::PortAllocator* allocator,
                    WebRtcChannelManager* channelmgr,
                    PeerConnection* connection,
                    talk_base::Thread* signaling_thread);

  ~WebRTCSessionImpl();
  virtual bool Initiate();
  virtual bool OnRemoteDescription(Json::Value& desc);
  virtual bool OnRemoteDescription(const cricket::SessionDescription* sdp,
                                   std::vector<cricket::Candidate>& candidates);
  virtual bool OnInitiateMessage(const cricket::SessionDescription* sdp,
                                 std::vector<cricket::Candidate>& candidates);
  virtual void OnMute(bool mute);
  virtual void OnCameraMute(bool mute);

  // Override from BaseSession to allow setting errors from other threads
  // than the signaling thread.
  virtual void SetError(Error error);

  bool muted() const { return muted_; }
  bool camera_muted() const { return camera_muted_; }

  bool CreateP2PTransportChannel(const std::string& stream_id, bool video);

  bool CreateVoiceChannel(const std::string& stream_id);
  bool CreateVideoChannel(const std::string& stream_id);
  bool RemoveStream(const std::string& stream_id);
  void RemoveAllStreams();

  // Returns true if we have either a voice or video stream matching this label.
  bool HasStream(const std::string& label) const;
  bool HasStream(bool video) const;

  // Returns true if there's one or more audio channels in the session.
  bool HasAudioStream() const;

  // Returns true if there's one or more video channels in the session.
  bool HasVideoStream() const;

  void OnCandidateReady(const cricket::Candidate& candidate);
  void OnStateChange(P2PTransportClass::State state,
                     cricket::TransportChannel* channel);
  void OnMessageReceived(const char* data, size_t data_size);
  bool SetVideoRenderer(const std::string& stream_id,
                        ExternalRenderer* external_renderer);
  bool SetVideoRenderer(int channel_id,
                        void* window,
                        unsigned int zOrder,
                        float left,
                        float top,
                        float right,
                        float bottom);
  sigslot::signal2<cricket::VideoChannel*, std::string&> SignalVideoChannel;
  sigslot::signal2<cricket::VoiceChannel*, std::string&> SignalVoiceChannel;
  sigslot::signal1<WebRTCSessionImpl*> SignalOnRemoveStream;

  void OnVoiceChannelCreated(cricket::VoiceChannel* voice_channel,
                             std::string& stream_id);
  void OnVideoChannelCreated(cricket::VideoChannel* video_channel,
                             std::string& stream_id);

  void ChannelEnable(cricket::BaseChannel* channel, bool enable);

  std::vector<cricket::Candidate>& local_candidates() {
    return local_candidates_;
  }

 private:
  bool SetVideoRenderer_w(int channel_id,
                          void* window,
                          unsigned int zOrder,
                          float left,
                          float top,
                          float right,
                          float bottom);
  void ChannelEnable_w(cricket::BaseChannel* channel, bool enable);

  void OnVoiceChannelError(cricket::VoiceChannel* voice_channel, uint32 ssrc,
                           cricket::VoiceMediaChannel::Error error);
  void OnVideoChannelError(cricket::VideoChannel* video_channel, uint32 ssrc,
                           cricket::VideoMediaChannel::Error error);

  // methods signaled by the transport
  void OnRequestSignaling(cricket::Transport* transport);
  void OnCandidatesReady(cricket::Transport* transport,
                         const std::vector<cricket::Candidate>& candidates);
  void OnWritableState(cricket::Transport* transport);

  // transport-management overrides from cricket::BaseSession
  virtual cricket::TransportChannel* CreateChannel(
      const std::string& content_name, const std::string& name);
  virtual cricket::TransportChannel* GetChannel(
      const std::string& content_name, const std::string& name);
  virtual void DestroyChannel(
      const std::string& content_name, const std::string& name);

  virtual talk_base::Thread* worker_thread() {
      return NULL;
  }
  void SendLocalDescription();

  void UpdateTransportWritableState();
  bool CheckAllTransportsWritable();
  void StartTransportTimeout(int timeout);
  void ClearTransportTimeout();
  void NotifyTransportState();

  cricket::SessionDescription* CreateOffer();
  cricket::SessionDescription* CreateAnswer(
      const cricket::SessionDescription* answer);

  //from MessageHandler
  virtual void OnMessage(talk_base::Message* message);

 private:
  typedef std::map<std::string, PC_Transport_Impl*> TransportChannelMap;

  cricket::VideoChannel* CreateVideoChannel_w(
      const std::string& content_name,
      bool rtcp,
      cricket::VoiceChannel* voice_channel);

  cricket::VoiceChannel* CreateVoiceChannel_w(
      const std::string& content_name,
      bool rtcp);

  void DestroyVoiceChannel_w(cricket::VoiceChannel* channel);
  void DestroyVideoChannel_w(cricket::VideoChannel* channel);
  void SignalOnWritableState_w(cricket::TransportChannel* channel);

  void SetSessionState(State state);
  void SetSessionState_w();
  bool SetVideoCapture(bool capture);
  cricket::CaptureResult SetVideoCapture_w(bool capture);
  void DisableLocalCandidate(const std::string& name);
  bool OnRemoteDescriptionUpdate(const cricket::SessionDescription* desc,
                                 std::vector<cricket::Candidate>& candidates);
  void RemoveStreamOnRequest(const cricket::Candidate& candidate);
  void RemoveStream_w(const std::string& stream_id);
  void RemoveAllStreams_w();

  void EnableAllStreams_w();

  void SendLocalDescription_w();

  WebRtcChannelManager* channel_manager_;
  std::vector<StreamInfo*> streams_;
  TransportChannelMap transport_channels_;
  bool all_writable_;
  bool muted_;
  bool camera_muted_;
  int setup_timeout_;
  std::vector<cricket::Candidate> local_candidates_;
  std::vector<cricket::Candidate> remote_candidates_;
  State session_state_;
  bool signal_initiated_;
};

} /* namespace webrtc */

#endif /* TALK_APP_WEBRTC_WEBRTCSESSIONIMPL_H_ */
