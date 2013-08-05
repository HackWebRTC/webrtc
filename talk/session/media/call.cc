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

#include <string>
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/base/window.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/screencastid.h"
#include "talk/p2p/base/parsing.h"
#include "talk/session/media/call.h"
#include "talk/session/media/mediasessionclient.h"

namespace cricket {

const uint32 MSG_CHECKAUTODESTROY = 1;
const uint32 MSG_TERMINATECALL = 2;
const uint32 MSG_PLAYDTMF = 3;

namespace {
const int kDTMFDelay = 300;  // msec
const size_t kMaxDTMFDigits = 30;
const int kSendToVoicemailTimeout = 1000*20;
const int kNoVoicemailTimeout = 1000*180;
const int kMediaMonitorInterval = 1000*15;
// In order to be the same as the server-side switching, this must be 100.
const int kAudioMonitorPollPeriodMillis = 100;

// V is a pointer type.
template<class K, class V>
V FindOrNull(const std::map<K, V>& map,
             const K& key) {
  typename std::map<K, V>::const_iterator it = map.find(key);
  return (it != map.end()) ? it->second : NULL;
}


bool ContentContainsCrypto(const cricket::ContentInfo* content) {
  if (content != NULL) {
    const cricket::MediaContentDescription* desc =
        static_cast<const cricket::MediaContentDescription*>(
            content->description);
    if (!desc || desc->cryptos().empty()) {
      return false;
    }
  }
  return true;
}

}

Call::Call(MediaSessionClient* session_client)
    : id_(talk_base::CreateRandomId()),
      session_client_(session_client),
      local_renderer_(NULL),
      has_video_(false),
      has_data_(false),
      muted_(false),
      video_muted_(false),
      send_to_voicemail_(true),
      playing_dtmf_(false) {
}

Call::~Call() {
  while (media_session_map_.begin() != media_session_map_.end()) {
    Session* session = media_session_map_.begin()->second.session;
    RemoveSession(session);
    session_client_->session_manager()->DestroySession(session);
  }
  talk_base::Thread::Current()->Clear(this);
}

Session* Call::InitiateSession(const buzz::Jid& to,
                               const buzz::Jid& initiator,
                               const CallOptions& options) {
  std::string id;
  std::string initiator_name = initiator.Str();
  return InternalInitiateSession(id, to, initiator_name, options);
}

Session *Call::InitiateSession(const std::string& id,
                               const buzz::Jid& to,
                               const CallOptions& options) {
  std::string initiator_name;
  return InternalInitiateSession(id, to, initiator_name, options);
}

void Call::IncomingSession(Session* session, const SessionDescription* offer) {
  AddSession(session, offer);

  // Make sure the session knows about the incoming ssrcs. This needs to be done
  // prior to the SignalSessionState call, because that may trigger handling of
  // these new SSRCs, so they need to be registered before then.
  UpdateRemoteMediaStreams(session, offer->contents(), false);

  // Missed the first state, the initiate, which is needed by
  // call_client.
  SignalSessionState(this, session, Session::STATE_RECEIVEDINITIATE);
}

void Call::AcceptSession(Session* session,
                         const cricket::CallOptions& options) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it != media_session_map_.end()) {
    const SessionDescription* answer = session_client_->CreateAnswer(
        session->remote_description(), options);
    it->second.session->Accept(answer);
  }
}

void Call::RejectSession(Session* session) {
  // Assume polite decline.
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it != media_session_map_.end())
    it->second.session->Reject(STR_TERMINATE_DECLINE);
}

void Call::TerminateSession(Session* session) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it != media_session_map_.end()) {
    // Assume polite terminations.
    it->second.session->Terminate();
  }
}

void Call::Terminate() {
  // Copy the list so that we can iterate over it in a stable way
  std::vector<Session*> sessions = this->sessions();

  // There may be more than one session to terminate
  std::vector<Session*>::iterator it;
  for (it = sessions.begin(); it != sessions.end(); ++it) {
    TerminateSession(*it);
  }
}

bool Call::SendViewRequest(Session* session,
                           const ViewRequest& view_request) {
  StaticVideoViews::const_iterator it;
  for (it = view_request.static_video_views.begin();
       it != view_request.static_video_views.end(); ++it) {
    StreamParams found_stream;
    bool found = false;
    MediaStreams* recv_streams = GetMediaStreams(session);
    if (recv_streams)
      found = recv_streams->GetVideoStream(it->selector, &found_stream);
    if (!found) {
      LOG(LS_WARNING) << "Trying to send view request for ("
                      << it->selector.ssrc << ", '"
                      << it->selector.groupid << "', '"
                      << it->selector.streamid << "'"
                      << ") is not in the local streams.";
      return false;
    }
  }

  XmlElements elems;
  WriteError error;
  if (!WriteJingleViewRequest(CN_VIDEO, view_request, &elems, &error)) {
    LOG(LS_ERROR) << "Couldn't write out view request: " << error.text;
    return false;
  }

  return session->SendInfoMessage(elems);
}

void Call::SetLocalRenderer(VideoRenderer* renderer) {
  local_renderer_ = renderer;
  if (session_client_->GetFocus() == this) {
    session_client_->channel_manager()->SetLocalRenderer(renderer);
  }
}

void Call::SetVideoRenderer(Session* session, uint32 ssrc,
                            VideoRenderer* renderer) {
  VideoChannel* video_channel = GetVideoChannel(session);
  if (video_channel) {
    video_channel->SetRenderer(ssrc, renderer);
    LOG(LS_INFO) << "Set renderer of ssrc " << ssrc
                 << " to " << renderer << ".";
  } else {
    LOG(LS_INFO) << "Failed to set renderer of ssrc " << ssrc << ".";
  }
}

void Call::OnMessage(talk_base::Message* message) {
  switch (message->message_id) {
  case MSG_CHECKAUTODESTROY:
    // If no more sessions for this call, delete it
    if (media_session_map_.empty())
      session_client_->DestroyCall(this);
    break;
  case MSG_TERMINATECALL:
    // Signal to the user that a timeout has happened and the call should
    // be sent to voicemail.
    if (send_to_voicemail_) {
      SignalSetupToCallVoicemail();
    }

    // Callee didn't answer - terminate call
    Terminate();
    break;
  case MSG_PLAYDTMF:
    ContinuePlayDTMF();
  }
}

std::vector<Session*> Call::sessions() {
  std::vector<Session*> sessions;
  MediaSessionMap::iterator it;
  for (it = media_session_map_.begin(); it != media_session_map_.end(); ++it)
    sessions.push_back(it->second.session);

  return sessions;
}

bool Call::AddSession(Session* session, const SessionDescription* offer) {
  bool succeeded = true;
  MediaSession media_session;
  media_session.session = session;
  media_session.voice_channel = NULL;
  media_session.video_channel = NULL;
  media_session.data_channel = NULL;
  media_session.recv_streams = NULL;

  const ContentInfo* audio_offer = GetFirstAudioContent(offer);
  const ContentInfo* video_offer = GetFirstVideoContent(offer);
  const ContentInfo* data_offer = GetFirstDataContent(offer);
  has_video_ = (video_offer != NULL);
  has_data_ = (data_offer != NULL);

  ASSERT(audio_offer != NULL);
  // Create voice channel and start a media monitor.
  media_session.voice_channel =
      session_client_->channel_manager()->CreateVoiceChannel(
          session, audio_offer->name, has_video_);
  // voice_channel can be NULL in case of NullVoiceEngine.
  if (media_session.voice_channel) {
    media_session.voice_channel->SignalMediaMonitor.connect(
        this, &Call::OnMediaMonitor);
    media_session.voice_channel->StartMediaMonitor(kMediaMonitorInterval);
  } else {
    succeeded = false;
  }

  // If desired, create video channel and start a media monitor.
  if (has_video_ && succeeded) {
    media_session.video_channel =
        session_client_->channel_manager()->CreateVideoChannel(
            session, video_offer->name, true, media_session.voice_channel);
    // video_channel can be NULL in case of NullVideoEngine.
    if (media_session.video_channel) {
      media_session.video_channel->SignalMediaMonitor.connect(
          this, &Call::OnMediaMonitor);
      media_session.video_channel->StartMediaMonitor(kMediaMonitorInterval);
    } else {
      succeeded = false;
    }
  }

  // If desired, create data channel.
  if (has_data_ && succeeded) {
    const DataContentDescription* data = GetFirstDataContentDescription(offer);
    if (data == NULL) {
      succeeded = false;
    } else {
      DataChannelType data_channel_type = DCT_RTP;
      if ((data->protocol() == kMediaProtocolSctp) ||
          (data->protocol() == kMediaProtocolDtlsSctp)) {
        data_channel_type = DCT_SCTP;
      }

      bool rtcp = false;
      media_session.data_channel =
          session_client_->channel_manager()->CreateDataChannel(
              session, data_offer->name, rtcp, data_channel_type);
      if (media_session.data_channel) {
        media_session.data_channel->SignalDataReceived.connect(
            this, &Call::OnDataReceived);
      } else {
        succeeded = false;
      }
    }
  }

  if (succeeded) {
    // Add session to list, create channels for this session.
    media_session.recv_streams = new MediaStreams;
    media_session_map_[session->id()] = media_session;
    session->SignalState.connect(this, &Call::OnSessionState);
    session->SignalError.connect(this, &Call::OnSessionError);
    session->SignalInfoMessage.connect(
        this, &Call::OnSessionInfoMessage);
    session->SignalRemoteDescriptionUpdate.connect(
        this, &Call::OnRemoteDescriptionUpdate);
    session->SignalReceivedTerminateReason
      .connect(this, &Call::OnReceivedTerminateReason);

    // If this call has the focus, enable this session's channels.
    if (session_client_->GetFocus() == this) {
      EnableSessionChannels(session, true);
    }

    // Signal client.
    SignalAddSession(this, session);
  }

  return succeeded;
}

void Call::RemoveSession(Session* session) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it == media_session_map_.end())
    return;

  // Remove all the screencasts, if they haven't been already.
  while (!it->second.started_screencasts.empty()) {
    uint32 ssrc = it->second.started_screencasts.begin()->first;
    if (!StopScreencastWithoutSendingUpdate(it->second.session, ssrc)) {
      LOG(LS_ERROR) << "Unable to stop screencast with ssrc " << ssrc;
      ASSERT(false);
    }
  }

  // Destroy video channel
  VideoChannel* video_channel = it->second.video_channel;
  if (video_channel != NULL)
    session_client_->channel_manager()->DestroyVideoChannel(video_channel);

  // Destroy voice channel
  VoiceChannel* voice_channel = it->second.voice_channel;
  if (voice_channel != NULL)
    session_client_->channel_manager()->DestroyVoiceChannel(voice_channel);

  // Destroy data channel
  DataChannel* data_channel = it->second.data_channel;
  if (data_channel != NULL)
    session_client_->channel_manager()->DestroyDataChannel(data_channel);

  delete it->second.recv_streams;
  media_session_map_.erase(it);

  // Destroy speaker monitor
  StopSpeakerMonitor(session);

  // Signal client
  SignalRemoveSession(this, session);

  // The call auto destroys when the last session is removed
  talk_base::Thread::Current()->Post(this, MSG_CHECKAUTODESTROY);
}

VoiceChannel* Call::GetVoiceChannel(Session* session) const {
  MediaSessionMap::const_iterator it = media_session_map_.find(session->id());
  return (it != media_session_map_.end()) ? it->second.voice_channel : NULL;
}

VideoChannel* Call::GetVideoChannel(Session* session) const {
  MediaSessionMap::const_iterator it = media_session_map_.find(session->id());
  return (it != media_session_map_.end()) ? it->second.video_channel : NULL;
}

DataChannel* Call::GetDataChannel(Session* session) const {
  MediaSessionMap::const_iterator it = media_session_map_.find(session->id());
  return (it != media_session_map_.end()) ? it->second.data_channel : NULL;
}

MediaStreams* Call::GetMediaStreams(Session* session) const {
  MediaSessionMap::const_iterator it = media_session_map_.find(session->id());
  return (it != media_session_map_.end()) ? it->second.recv_streams : NULL;
}

void Call::EnableChannels(bool enable) {
  MediaSessionMap::iterator it;
  for (it = media_session_map_.begin(); it != media_session_map_.end(); ++it) {
    EnableSessionChannels(it->second.session, enable);
  }
  session_client_->channel_manager()->SetLocalRenderer(
      (enable) ? local_renderer_ : NULL);
}

void Call::EnableSessionChannels(Session* session, bool enable) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it == media_session_map_.end())
    return;

  VoiceChannel* voice_channel = it->second.voice_channel;
  VideoChannel* video_channel = it->second.video_channel;
  DataChannel* data_channel = it->second.data_channel;
  if (voice_channel != NULL)
    voice_channel->Enable(enable);
  if (video_channel != NULL)
    video_channel->Enable(enable);
  if (data_channel != NULL)
    data_channel->Enable(enable);
}

void Call::Mute(bool mute) {
  muted_ = mute;
  MediaSessionMap::iterator it;
  for (it = media_session_map_.begin(); it != media_session_map_.end(); ++it) {
    if (it->second.voice_channel != NULL)
      it->second.voice_channel->MuteStream(0, mute);
  }
}

void Call::MuteVideo(bool mute) {
  video_muted_ = mute;
  MediaSessionMap::iterator it;
  for (it = media_session_map_.begin(); it != media_session_map_.end(); ++it) {
    if (it->second.video_channel != NULL)
      it->second.video_channel->MuteStream(0, mute);
  }
}

bool Call::SendData(Session* session,
                    const SendDataParams& params,
                    const talk_base::Buffer& payload,
                    SendDataResult* result) {
  DataChannel* data_channel = GetDataChannel(session);
  if (!data_channel) {
    LOG(LS_WARNING) << "Could not send data: no data channel.";
    return false;
  }

  return data_channel->SendData(params, payload, result);
}

void Call::PressDTMF(int event) {
  // Queue up this digit
  if (queued_dtmf_.size() < kMaxDTMFDigits) {
    LOG(LS_INFO) << "Call::PressDTMF(" << event << ")";

    queued_dtmf_.push_back(event);

    if (!playing_dtmf_) {
      ContinuePlayDTMF();
    }
  }
}

cricket::VideoFormat ScreencastFormatFromFps(int fps) {
  // The capturer pretty much ignore this, but just in case we give it
  // a resolution big enough to cover any expected desktop.  In any
  // case, it can't be 0x0, or the CaptureManager will fail to use it.
  return cricket::VideoFormat(
      1, 1,
      cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_ANY);
}

bool Call::StartScreencast(Session* session,
                           const std::string& streamid, uint32 ssrc,
                           const ScreencastId& screencastid, int fps) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it == media_session_map_.end()) {
    return false;
  }

  VideoChannel *video_channel = GetVideoChannel(session);
  if (!video_channel) {
    LOG(LS_WARNING) << "Cannot add screencast"
                    << " because there is no video channel.";
    return false;
  }

  VideoCapturer *capturer = video_channel->AddScreencast(ssrc, screencastid);
  if (capturer == NULL) {
    LOG(LS_WARNING) << "Could not create screencast capturer.";
    return false;
  }

  VideoFormat format = ScreencastFormatFromFps(fps);
  if (!session_client_->channel_manager()->StartVideoCapture(
          capturer, format)) {
    LOG(LS_WARNING) << "Could not start video capture.";
    video_channel->RemoveScreencast(ssrc);
    return false;
  }

  if (!video_channel->SetCapturer(ssrc, capturer)) {
    LOG(LS_WARNING) << "Could not start sending screencast.";
    session_client_->channel_manager()->StopVideoCapture(
        capturer, ScreencastFormatFromFps(fps));
    video_channel->RemoveScreencast(ssrc);
  }

  // TODO(pthatcher): Once the CaptureManager has a nicer interface
  // for removing captures (such as having StartCapture return a
  // handle), remove this StartedCapture stuff.
  it->second.started_screencasts.insert(
      std::make_pair(ssrc, StartedCapture(capturer, format)));

  // TODO(pthatcher): Verify we aren't re-using an existing id or
  // ssrc.
  StreamParams stream;
  stream.id = streamid;
  stream.ssrcs.push_back(ssrc);
  VideoContentDescription* video = CreateVideoStreamUpdate(stream);

  // TODO(pthatcher): Wait until view request before sending video.
  video_channel->SetLocalContent(video, CA_UPDATE);
  SendVideoStreamUpdate(session, video);
  return true;
}

bool Call::StopScreencast(Session* session,
                          const std::string& streamid, uint32 ssrc) {
  if (!StopScreencastWithoutSendingUpdate(session, ssrc)) {
    return false;
  }

  VideoChannel *video_channel = GetVideoChannel(session);
  if (!video_channel) {
    LOG(LS_WARNING) << "Cannot add screencast"
                    << " because there is no video channel.";
    return false;
  }

  StreamParams stream;
  stream.id = streamid;
  // No ssrcs
  VideoContentDescription* video = CreateVideoStreamUpdate(stream);

  video_channel->SetLocalContent(video, CA_UPDATE);
  SendVideoStreamUpdate(session, video);
  return true;
}

bool Call::StopScreencastWithoutSendingUpdate(
    Session* session, uint32 ssrc) {
  MediaSessionMap::iterator it = media_session_map_.find(session->id());
  if (it == media_session_map_.end()) {
    return false;
  }

  VideoChannel *video_channel = GetVideoChannel(session);
  if (!video_channel) {
    LOG(LS_WARNING) << "Cannot remove screencast"
                    << " because there is no video channel.";
    return false;
  }

  StartedScreencastMap::const_iterator screencast_iter =
      it->second.started_screencasts.find(ssrc);
  if (screencast_iter == it->second.started_screencasts.end()) {
    LOG(LS_WARNING) << "Could not stop screencast " << ssrc
                    << " because there is no capturer.";
    return false;
  }

  VideoCapturer* capturer = screencast_iter->second.capturer;
  VideoFormat format = screencast_iter->second.format;
  video_channel->SetCapturer(ssrc, NULL);
  if (!session_client_->channel_manager()->StopVideoCapture(
          capturer, format)) {
    LOG(LS_WARNING) << "Could not stop screencast " << ssrc
                    << " because could not stop capture.";
    return false;
  }
  video_channel->RemoveScreencast(ssrc);
  it->second.started_screencasts.erase(ssrc);
  return true;
}

VideoContentDescription* Call::CreateVideoStreamUpdate(
    const StreamParams& stream) {
  VideoContentDescription* video = new VideoContentDescription();
  video->set_multistream(true);
  video->set_partial(true);
  video->AddStream(stream);
  return video;
}

void Call::SendVideoStreamUpdate(
    Session* session, VideoContentDescription* video) {
  // Takes the ownership of |video|.
  talk_base::scoped_ptr<VideoContentDescription> description(video);
  const ContentInfo* video_info =
      GetFirstVideoContent(session->local_description());
  if (video_info == NULL) {
    LOG(LS_WARNING) << "Cannot send stream update for video.";
    return;
  }

  std::vector<ContentInfo> contents;
  contents.push_back(
      ContentInfo(video_info->name, video_info->type, description.get()));

  session->SendDescriptionInfoMessage(contents);
}

void Call::ContinuePlayDTMF() {
  playing_dtmf_ = false;

  // Check to see if we have a queued tone
  if (queued_dtmf_.size() > 0) {
    playing_dtmf_ = true;

    int tone = queued_dtmf_.front();
    queued_dtmf_.pop_front();

    LOG(LS_INFO) << "Call::ContinuePlayDTMF(" << tone << ")";
    for (MediaSessionMap::iterator it = media_session_map_.begin();
         it != media_session_map_.end(); ++it) {
      if (it->second.voice_channel != NULL) {
        it->second.voice_channel->PressDTMF(tone, true);
      }
    }

    // Post a message to play the next tone or at least clear the playing_dtmf_
    // bit.
    talk_base::Thread::Current()->PostDelayed(kDTMFDelay, this, MSG_PLAYDTMF);
  }
}

void Call::Join(Call* call, bool enable) {
  for (MediaSessionMap::iterator it = call->media_session_map_.begin();
       it != call->media_session_map_.end(); ++it) {
    // Shouldn't already exist.
    ASSERT(media_session_map_.find(it->first) == media_session_map_.end());
    media_session_map_[it->first] = it->second;

    it->second.session->SignalState.connect(this, &Call::OnSessionState);
    it->second.session->SignalError.connect(this, &Call::OnSessionError);
    it->second.session->SignalReceivedTerminateReason
      .connect(this, &Call::OnReceivedTerminateReason);

    EnableSessionChannels(it->second.session, enable);
  }

  // Moved all the sessions over, so the other call should no longer have any.
  call->media_session_map_.clear();
}

void Call::StartConnectionMonitor(Session* session, int cms) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (voice_channel) {
    voice_channel->SignalConnectionMonitor.connect(this,
        &Call::OnConnectionMonitor);
    voice_channel->StartConnectionMonitor(cms);
  }

  VideoChannel* video_channel = GetVideoChannel(session);
  if (video_channel) {
    video_channel->SignalConnectionMonitor.connect(this,
        &Call::OnConnectionMonitor);
    video_channel->StartConnectionMonitor(cms);
  }
}

void Call::StopConnectionMonitor(Session* session) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (voice_channel) {
    voice_channel->StopConnectionMonitor();
    voice_channel->SignalConnectionMonitor.disconnect(this);
  }

  VideoChannel* video_channel = GetVideoChannel(session);
  if (video_channel) {
    video_channel->StopConnectionMonitor();
    video_channel->SignalConnectionMonitor.disconnect(this);
  }
}

void Call::StartAudioMonitor(Session* session, int cms) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (voice_channel) {
    voice_channel->SignalAudioMonitor.connect(this, &Call::OnAudioMonitor);
    voice_channel->StartAudioMonitor(cms);
  }
}

void Call::StopAudioMonitor(Session* session) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (voice_channel) {
    voice_channel->StopAudioMonitor();
    voice_channel->SignalAudioMonitor.disconnect(this);
  }
}

bool Call::IsAudioMonitorRunning(Session* session) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (voice_channel) {
    return voice_channel->IsAudioMonitorRunning();
  } else {
    return false;
  }
}

void Call::StartSpeakerMonitor(Session* session) {
  if (speaker_monitor_map_.find(session->id()) == speaker_monitor_map_.end()) {
    if (!IsAudioMonitorRunning(session)) {
      StartAudioMonitor(session, kAudioMonitorPollPeriodMillis);
    }
    CurrentSpeakerMonitor* speaker_monitor =
        new cricket::CurrentSpeakerMonitor(this, session);
    speaker_monitor->SignalUpdate.connect(this, &Call::OnSpeakerMonitor);
    speaker_monitor->Start();
    speaker_monitor_map_[session->id()] = speaker_monitor;
  } else {
    LOG(LS_WARNING) << "Already started speaker monitor for session "
                    << session->id() << ".";
  }
}

void Call::StopSpeakerMonitor(Session* session) {
  if (speaker_monitor_map_.find(session->id()) == speaker_monitor_map_.end()) {
    LOG(LS_WARNING) << "Speaker monitor for session "
                    << session->id() << " already stopped.";
  } else {
    CurrentSpeakerMonitor* monitor = speaker_monitor_map_[session->id()];
    monitor->Stop();
    speaker_monitor_map_.erase(session->id());
    delete monitor;
  }
}

void Call::OnConnectionMonitor(VoiceChannel* channel,
                               const std::vector<ConnectionInfo> &infos) {
  SignalConnectionMonitor(this, infos);
}

void Call::OnMediaMonitor(VoiceChannel* channel, const VoiceMediaInfo& info) {
  last_voice_media_info_ = info;
  SignalMediaMonitor(this, info);
}

void Call::OnAudioMonitor(VoiceChannel* channel, const AudioInfo& info) {
  SignalAudioMonitor(this, info);
}

void Call::OnSpeakerMonitor(CurrentSpeakerMonitor* monitor, uint32 ssrc) {
  Session* session = static_cast<Session*>(monitor->session());
  MediaStreams* recv_streams = GetMediaStreams(session);
  if (recv_streams) {
    StreamParams stream;
    recv_streams->GetAudioStream(StreamSelector(ssrc), &stream);
    SignalSpeakerMonitor(this, session, stream);
  }
}

void Call::OnConnectionMonitor(VideoChannel* channel,
                               const std::vector<ConnectionInfo> &infos) {
  SignalVideoConnectionMonitor(this, infos);
}

void Call::OnMediaMonitor(VideoChannel* channel, const VideoMediaInfo& info) {
  SignalVideoMediaMonitor(this, info);
}

void Call::OnDataReceived(DataChannel* channel,
                          const ReceiveDataParams& params,
                          const talk_base::Buffer& payload) {
  SignalDataReceived(this, params, payload);
}

uint32 Call::id() {
  return id_;
}

void Call::OnSessionState(BaseSession* base_session, BaseSession::State state) {
  Session* session = static_cast<Session*>(base_session);
  switch (state) {
    case Session::STATE_RECEIVEDACCEPT:
      UpdateRemoteMediaStreams(session,
          session->remote_description()->contents(), false);
      session_client_->session_manager()->signaling_thread()->Clear(this,
          MSG_TERMINATECALL);
      break;
    case Session::STATE_RECEIVEDREJECT:
    case Session::STATE_RECEIVEDTERMINATE:
      session_client_->session_manager()->signaling_thread()->Clear(this,
          MSG_TERMINATECALL);
      break;
    default:
      break;
  }
  SignalSessionState(this, session, state);
}

void Call::OnSessionError(BaseSession* base_session, Session::Error error) {
  session_client_->session_manager()->signaling_thread()->Clear(this,
      MSG_TERMINATECALL);
  SignalSessionError(this, static_cast<Session*>(base_session), error);
}

void Call::OnSessionInfoMessage(Session* session,
                                const buzz::XmlElement* action_elem) {
  if (!IsJingleViewRequest(action_elem)) {
    return;
  }

  ViewRequest view_request;
  ParseError error;
  if (!ParseJingleViewRequest(action_elem, &view_request, &error)) {
    LOG(LS_WARNING) << "Failed to parse view request: " << error.text;
    return;
  }

  VideoChannel* video_channel = GetVideoChannel(session);
  if (video_channel == NULL) {
    LOG(LS_WARNING) << "Ignore view request since we have no video channel.";
    return;
  }

  if (!video_channel->ApplyViewRequest(view_request)) {
    LOG(LS_WARNING) << "Failed to ApplyViewRequest.";
  }
}

void Call::OnRemoteDescriptionUpdate(BaseSession* base_session,
                                     const ContentInfos& updated_contents) {
  Session* session = static_cast<Session*>(base_session);

  const ContentInfo* audio_content = GetFirstAudioContent(updated_contents);
  if (audio_content) {
    const AudioContentDescription* audio_update =
        static_cast<const AudioContentDescription*>(audio_content->description);
    if (!audio_update->codecs().empty()) {
      UpdateVoiceChannelRemoteContent(session, audio_update);
    }
  }

  const ContentInfo* video_content = GetFirstVideoContent(updated_contents);
  if (video_content) {
    const VideoContentDescription* video_update =
        static_cast<const VideoContentDescription*>(video_content->description);
    if (!video_update->codecs().empty()) {
      UpdateVideoChannelRemoteContent(session, video_update);
    }
  }

  const ContentInfo* data_content = GetFirstDataContent(updated_contents);
  if (data_content) {
    const DataContentDescription* data_update =
        static_cast<const DataContentDescription*>(data_content->description);
    if (!data_update->codecs().empty()) {
      UpdateDataChannelRemoteContent(session, data_update);
    }
  }

  UpdateRemoteMediaStreams(session, updated_contents, true);
}

bool Call::UpdateVoiceChannelRemoteContent(
    Session* session, const AudioContentDescription* audio) {
  VoiceChannel* voice_channel = GetVoiceChannel(session);
  if (!voice_channel->SetRemoteContent(audio, CA_UPDATE)) {
    LOG(LS_ERROR) << "Failure in audio SetRemoteContent with CA_UPDATE";
    session->SetError(BaseSession::ERROR_CONTENT);
    return false;
  }
  return true;
}

bool Call::UpdateVideoChannelRemoteContent(
    Session* session, const VideoContentDescription* video) {
  VideoChannel* video_channel = GetVideoChannel(session);
  if (!video_channel->SetRemoteContent(video, CA_UPDATE)) {
    LOG(LS_ERROR) << "Failure in video SetRemoteContent with CA_UPDATE";
    session->SetError(BaseSession::ERROR_CONTENT);
    return false;
  }
  return true;
}

bool Call::UpdateDataChannelRemoteContent(
    Session* session, const DataContentDescription* data) {
  DataChannel* data_channel = GetDataChannel(session);
  if (!data_channel->SetRemoteContent(data, CA_UPDATE)) {
    LOG(LS_ERROR) << "Failure in data SetRemoteContent with CA_UPDATE";
    session->SetError(BaseSession::ERROR_CONTENT);
    return false;
  }
  return true;
}

void Call::UpdateRemoteMediaStreams(Session* session,
                                    const ContentInfos& updated_contents,
                                    bool update_channels) {
  MediaStreams* recv_streams = GetMediaStreams(session);
  if (!recv_streams)
    return;

  cricket::MediaStreams added_streams;
  cricket::MediaStreams removed_streams;

  const ContentInfo* audio_content = GetFirstAudioContent(updated_contents);
  if (audio_content) {
    const AudioContentDescription* audio_update =
        static_cast<const AudioContentDescription*>(audio_content->description);
    UpdateRecvStreams(audio_update->streams(),
                      update_channels ? GetVoiceChannel(session) : NULL,
                      recv_streams->mutable_audio(),
                      added_streams.mutable_audio(),
                      removed_streams.mutable_audio());
  }

  const ContentInfo* video_content = GetFirstVideoContent(updated_contents);
  if (video_content) {
    const VideoContentDescription* video_update =
        static_cast<const VideoContentDescription*>(video_content->description);
    UpdateRecvStreams(video_update->streams(),
                      update_channels ? GetVideoChannel(session) : NULL,
                      recv_streams->mutable_video(),
                      added_streams.mutable_video(),
                      removed_streams.mutable_video());
  }

  const ContentInfo* data_content = GetFirstDataContent(updated_contents);
  if (data_content) {
    const DataContentDescription* data_update =
        static_cast<const DataContentDescription*>(data_content->description);
    UpdateRecvStreams(data_update->streams(),
                      update_channels ? GetDataChannel(session) : NULL,
                      recv_streams->mutable_data(),
                      added_streams.mutable_data(),
                      removed_streams.mutable_data());
  }

  if (!added_streams.empty() || !removed_streams.empty()) {
    SignalMediaStreamsUpdate(this, session, added_streams, removed_streams);
  }
}

void FindStreamChanges(const std::vector<StreamParams>& streams,
                       const std::vector<StreamParams>& updates,
                       std::vector<StreamParams>* added_streams,
                       std::vector<StreamParams>* removed_streams) {
  for (std::vector<StreamParams>::const_iterator update = updates.begin();
       update != updates.end(); ++update) {
    StreamParams stream;
    if (GetStreamByIds(streams, update->groupid, update->id, &stream)) {
      if (!update->has_ssrcs()) {
        removed_streams->push_back(stream);
      }
    } else {
      // There's a bug on reflector that will send <stream>s even
      // though there is not ssrc (which means there isn't really a
      // stream).  To work around it, we simply ignore new <stream>s
      // that don't have any ssrcs.
      if (update->has_ssrcs()) {
        added_streams->push_back(*update);
      }
    }
  }
}

void Call::UpdateRecvStreams(const std::vector<StreamParams>& update_streams,
                             BaseChannel* channel,
                             std::vector<StreamParams>* recv_streams,
                             std::vector<StreamParams>* added_streams,
                             std::vector<StreamParams>* removed_streams) {
  FindStreamChanges(*recv_streams,
                    update_streams, added_streams, removed_streams);
  AddRecvStreams(*added_streams,
                 channel, recv_streams);
  RemoveRecvStreams(*removed_streams,
                    channel, recv_streams);
}

void Call::AddRecvStreams(const std::vector<StreamParams>& added_streams,
                          BaseChannel* channel,
                          std::vector<StreamParams>* recv_streams) {
  std::vector<StreamParams>::const_iterator stream;
  for (stream = added_streams.begin();
       stream != added_streams.end();
       ++stream) {
    AddRecvStream(*stream, channel, recv_streams);
  }
}

void Call::AddRecvStream(const StreamParams& stream,
                         BaseChannel* channel,
                         std::vector<StreamParams>* recv_streams) {
  if (channel && stream.has_ssrcs()) {
    channel->AddRecvStream(stream);
  }
  recv_streams->push_back(stream);
}

void Call::RemoveRecvStreams(const std::vector<StreamParams>& removed_streams,
                             BaseChannel* channel,
                             std::vector<StreamParams>* recv_streams) {
  std::vector<StreamParams>::const_iterator stream;
  for (stream = removed_streams.begin();
       stream != removed_streams.end();
       ++stream) {
    RemoveRecvStream(*stream, channel, recv_streams);
  }
}

void Call::RemoveRecvStream(const StreamParams& stream,
                            BaseChannel* channel,
                            std::vector<StreamParams>* recv_streams) {
  if (channel && stream.has_ssrcs()) {
    // TODO(pthatcher): Change RemoveRecvStream to take a stream argument.
    channel->RemoveRecvStream(stream.first_ssrc());
  }
  RemoveStreamByIds(recv_streams, stream.groupid, stream.id);
}

void Call::OnReceivedTerminateReason(Session* session,
                                     const std::string& reason) {
  session_client_->session_manager()->signaling_thread()->Clear(this,
    MSG_TERMINATECALL);
  SignalReceivedTerminateReason(this, session, reason);
}

// TODO(mdodd): Get ride of this method since all Hangouts are using a secure
// connection.
bool Call::secure() const {
  if (session_client_->secure() == SEC_DISABLED) {
    return false;
  }

  bool ret = true;
  int i = 0;

  MediaSessionMap::const_iterator it;
  for (it = media_session_map_.begin(); it != media_session_map_.end(); ++it) {
    LOG_F(LS_VERBOSE) << "session[" << i
                      << "], check local and remote descriptions";
    i++;

    if (!SessionDescriptionContainsCrypto(
            it->second.session->local_description()) ||
        !SessionDescriptionContainsCrypto(
            it->second.session->remote_description())) {
      ret = false;
      break;
    }
  }

  LOG_F(LS_VERBOSE) << "secure=" << ret;
  return ret;
}

bool Call::SessionDescriptionContainsCrypto(
    const SessionDescription* sdesc) const {
  if (sdesc == NULL) {
    LOG_F(LS_VERBOSE) << "sessionDescription is NULL";
    return false;
  }

  return ContentContainsCrypto(sdesc->GetContentByName(CN_AUDIO)) &&
         ContentContainsCrypto(sdesc->GetContentByName(CN_VIDEO));
}

Session* Call::InternalInitiateSession(const std::string& id,
                                       const buzz::Jid& to,
                                       const std::string& initiator_name,
                                       const CallOptions& options) {
  const SessionDescription* offer = session_client_->CreateOffer(options);

  Session* session = session_client_->CreateSession(id, this);
  session->set_initiator_name(initiator_name);

  AddSession(session, offer);
  session->Initiate(to.Str(), offer);

  // After this timeout, terminate the call because the callee isn't
  // answering
  session_client_->session_manager()->signaling_thread()->Clear(this,
      MSG_TERMINATECALL);
  session_client_->session_manager()->signaling_thread()->PostDelayed(
    send_to_voicemail_ ? kSendToVoicemailTimeout : kNoVoicemailTimeout,
    this, MSG_TERMINATECALL);
  return session;
}

}  // namespace cricket
