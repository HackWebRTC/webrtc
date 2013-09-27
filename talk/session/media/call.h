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

#ifndef TALK_SESSION_MEDIA_CALL_H_
#define TALK_SESSION_MEDIA_CALL_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "talk/base/messagequeue.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/screencastid.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/videocommon.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/client/socketmonitor.h"
#include "talk/session/media/audiomonitor.h"
#include "talk/session/media/currentspeakermonitor.h"
#include "talk/session/media/mediamessages.h"
#include "talk/session/media/mediasession.h"
#include "talk/xmpp/jid.h"

namespace cricket {

class MediaSessionClient;
class BaseChannel;
class VoiceChannel;
class VideoChannel;
class DataChannel;

// Can't typedef this easily since it's forward declared as struct elsewhere.
struct CallOptions : public MediaSessionOptions {
};

class Call : public talk_base::MessageHandler, public sigslot::has_slots<> {
 public:
  explicit Call(MediaSessionClient* session_client);
  ~Call();

  // |initiator| can be empty.
  Session* InitiateSession(const buzz::Jid& to, const buzz::Jid& initiator,
                           const CallOptions& options);
  Session* InitiateSession(const std::string& id, const buzz::Jid& to,
                           const CallOptions& options);
  void AcceptSession(Session* session, const CallOptions& options);
  void RejectSession(Session* session);
  void TerminateSession(Session* session);
  void Terminate();
  bool SendViewRequest(Session* session,
                       const ViewRequest& view_request);
  void SetLocalRenderer(VideoRenderer* renderer);
  void SetVideoRenderer(Session* session, uint32 ssrc,
                        VideoRenderer* renderer);
  void StartConnectionMonitor(Session* session, int cms);
  void StopConnectionMonitor(Session* session);
  void StartAudioMonitor(Session* session, int cms);
  void StopAudioMonitor(Session* session);
  bool IsAudioMonitorRunning(Session* session);
  void StartSpeakerMonitor(Session* session);
  void StopSpeakerMonitor(Session* session);
  void Mute(bool mute);
  void MuteVideo(bool mute);
  bool SendData(Session* session,
                const SendDataParams& params,
                const talk_base::Buffer& payload,
                SendDataResult* result);
  void PressDTMF(int event);
  bool StartScreencast(Session* session,
                       const std::string& stream_name, uint32 ssrc,
                       const ScreencastId& screencastid, int fps);
  bool StopScreencast(Session* session,
                      const std::string& stream_name, uint32 ssrc);

  std::vector<Session*> sessions();
  uint32 id();
  bool has_video() const { return has_video_; }
  bool has_data() const { return has_data_; }
  bool muted() const { return muted_; }
  bool video() const { return has_video_; }
  bool secure() const;
  bool video_muted() const { return video_muted_; }
  const std::vector<StreamParams>* GetDataRecvStreams(Session* session) const {
    MediaStreams* recv_streams = GetMediaStreams(session);
    return recv_streams ? &recv_streams->data() : NULL;
  }
  const std::vector<StreamParams>* GetVideoRecvStreams(Session* session) const {
    MediaStreams* recv_streams = GetMediaStreams(session);
    return recv_streams ? &recv_streams->video() : NULL;
  }
  const std::vector<StreamParams>* GetAudioRecvStreams(Session* session) const {
    MediaStreams* recv_streams = GetMediaStreams(session);
    return recv_streams ? &recv_streams->audio() : NULL;
  }
  VoiceChannel* GetVoiceChannel(Session* session) const;
  VideoChannel* GetVideoChannel(Session* session) const;
  DataChannel* GetDataChannel(Session* session) const;
  // Public just for unit tests
  VideoContentDescription* CreateVideoStreamUpdate(const StreamParams& stream);
  // Takes ownership of video.
  void SendVideoStreamUpdate(Session* session, VideoContentDescription* video);

  // Setting this to false will cause the call to have a longer timeout and
  // for the SignalSetupToCallVoicemail to never fire.
  void set_send_to_voicemail(bool send_to_voicemail) {
    send_to_voicemail_ = send_to_voicemail;
  }
  bool send_to_voicemail() { return send_to_voicemail_; }
  const VoiceMediaInfo& last_voice_media_info() const {
    return last_voice_media_info_;
  }

  // Sets a flag on the chatapp that will redirect the call to voicemail once
  // the call has been terminated
  sigslot::signal0<> SignalSetupToCallVoicemail;
  sigslot::signal2<Call*, Session*> SignalAddSession;
  sigslot::signal2<Call*, Session*> SignalRemoveSession;
  sigslot::signal3<Call*, Session*, Session::State>
      SignalSessionState;
  sigslot::signal3<Call*, Session*, Session::Error>
      SignalSessionError;
  sigslot::signal3<Call*, Session*, const std::string &>
      SignalReceivedTerminateReason;
  sigslot::signal2<Call*, const std::vector<ConnectionInfo> &>
      SignalConnectionMonitor;
  sigslot::signal2<Call*, const VoiceMediaInfo&> SignalMediaMonitor;
  sigslot::signal2<Call*, const AudioInfo&> SignalAudioMonitor;
  // Empty nick on StreamParams means "unknown".
  // No ssrcs in StreamParams means "no current speaker".
  sigslot::signal3<Call*,
                   Session*,
                   const StreamParams&> SignalSpeakerMonitor;
  sigslot::signal2<Call*, const std::vector<ConnectionInfo> &>
      SignalVideoConnectionMonitor;
  sigslot::signal2<Call*, const VideoMediaInfo&> SignalVideoMediaMonitor;
  // Gives added streams and removed streams, in that order.
  sigslot::signal4<Call*,
                   Session*,
                   const MediaStreams&,
                   const MediaStreams&> SignalMediaStreamsUpdate;
  sigslot::signal3<Call*,
                   const ReceiveDataParams&,
                   const talk_base::Buffer&> SignalDataReceived;

 private:
  void OnMessage(talk_base::Message* message);
  void OnSessionState(BaseSession* base_session, BaseSession::State state);
  void OnSessionError(BaseSession* base_session, Session::Error error);
  void OnSessionInfoMessage(
      Session* session, const buzz::XmlElement* action_elem);
  void OnViewRequest(
      Session* session, const ViewRequest& view_request);
  void OnRemoteDescriptionUpdate(
      BaseSession* base_session, const ContentInfos& updated_contents);
  void OnReceivedTerminateReason(Session* session, const std::string &reason);
  void IncomingSession(Session* session, const SessionDescription* offer);
  // Returns true on success.
  bool AddSession(Session* session, const SessionDescription* offer);
  void RemoveSession(Session* session);
  void EnableChannels(bool enable);
  void EnableSessionChannels(Session* session, bool enable);
  void Join(Call* call, bool enable);
  void OnConnectionMonitor(VoiceChannel* channel,
                           const std::vector<ConnectionInfo> &infos);
  void OnMediaMonitor(VoiceChannel* channel, const VoiceMediaInfo& info);
  void OnAudioMonitor(VoiceChannel* channel, const AudioInfo& info);
  void OnSpeakerMonitor(CurrentSpeakerMonitor* monitor, uint32 ssrc);
  void OnConnectionMonitor(VideoChannel* channel,
                           const std::vector<ConnectionInfo> &infos);
  void OnMediaMonitor(VideoChannel* channel, const VideoMediaInfo& info);
  void OnDataReceived(DataChannel* channel,
                      const ReceiveDataParams& params,
                      const talk_base::Buffer& payload);
  MediaStreams* GetMediaStreams(Session* session) const;
  void UpdateRemoteMediaStreams(Session* session,
                                const ContentInfos& updated_contents,
                                bool update_channels);
  bool UpdateVoiceChannelRemoteContent(Session* session,
                                       const AudioContentDescription* audio);
  bool UpdateVideoChannelRemoteContent(Session* session,
                                       const VideoContentDescription* video);
  bool UpdateDataChannelRemoteContent(Session* session,
                                      const DataContentDescription* data);
  void UpdateRecvStreams(const std::vector<StreamParams>& update_streams,
                         BaseChannel* channel,
                         std::vector<StreamParams>* recv_streams,
                         std::vector<StreamParams>* added_streams,
                         std::vector<StreamParams>* removed_streams);
  void AddRecvStreams(const std::vector<StreamParams>& added_streams,
                      BaseChannel* channel,
                      std::vector<StreamParams>* recv_streams);
  void AddRecvStream(const StreamParams& stream,
                     BaseChannel* channel,
                     std::vector<StreamParams>* recv_streams);
  void RemoveRecvStreams(const std::vector<StreamParams>& removed_streams,
                         BaseChannel* channel,
                         std::vector<StreamParams>* recv_streams);
  void RemoveRecvStream(const StreamParams& stream,
                        BaseChannel* channel,
                        std::vector<StreamParams>* recv_streams);
  void ContinuePlayDTMF();
  bool StopScreencastWithoutSendingUpdate(Session* session, uint32 ssrc);
  bool StopAllScreencastsWithoutSendingUpdate(Session* session);
  bool SessionDescriptionContainsCrypto(const SessionDescription* sdesc) const;
  Session* InternalInitiateSession(const std::string& id,
                                   const buzz::Jid& to,
                                   const std::string& initiator_name,
                                   const CallOptions& options);

  uint32 id_;
  MediaSessionClient* session_client_;

  struct StartedCapture {
    StartedCapture(cricket::VideoCapturer* capturer,
                   const cricket::VideoFormat& format) :
        capturer(capturer),
        format(format) {
    }
    cricket::VideoCapturer* capturer;
    cricket::VideoFormat format;
  };
  typedef std::map<uint32, StartedCapture> StartedScreencastMap;

  struct MediaSession {
    Session* session;
    VoiceChannel* voice_channel;
    VideoChannel* video_channel;
    DataChannel* data_channel;
    MediaStreams* recv_streams;
    StartedScreencastMap started_screencasts;
  };

  // Create a map of media sessions, keyed off session->id().
  typedef std::map<std::string, MediaSession> MediaSessionMap;
  MediaSessionMap media_session_map_;

  std::map<std::string, CurrentSpeakerMonitor*> speaker_monitor_map_;
  VideoRenderer* local_renderer_;
  bool has_video_;
  bool has_data_;
  bool muted_;
  bool video_muted_;
  bool send_to_voicemail_;

  // DTMF tones have to be queued up so that we don't flood the call.  We
  // keep a deque (doubely ended queue) of them around.  While one is playing we
  // set the playing_dtmf_ bit and schedule a message in XX msec to clear that
  // bit or start the next tone playing.
  std::deque<int> queued_dtmf_;
  bool playing_dtmf_;

  VoiceMediaInfo last_voice_media_info_;

  friend class MediaSessionClient;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CALL_H_
