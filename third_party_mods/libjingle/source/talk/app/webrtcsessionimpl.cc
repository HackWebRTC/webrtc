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
#include "talk/app/webrtcsessionimpl.h"

#include <string>
#include <vector>

#include "talk/app/pc_transport_impl.h"
#include "talk/app/peerconnection.h"
#include "talk/app/webrtc_json.h"
#include "talk/base/common.h"
#include "talk/base/json.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/p2ptransport.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/session/phone/voicechannel.h"

using namespace cricket;

namespace webrtc {

enum {
  MSG_RTC_CREATEVIDEOCHANNEL = 1,
  MSG_RTC_CREATEAUDIOCHANNEL = 2,
  MSG_RTC_SETSTATE = 3,
  MSG_RTC_SETVIDEOCAPTURE = 4,
  MSG_RTC_CANDIDATETIMEOUT = 5,
  MSG_RTC_SETEXTERNALRENDERER = 6,
  MSG_RTC_SETCRICKETRENDERER = 7,
  MSG_RTC_CHANNELENABLE = 8,
  MSG_RTC_SIGNALONWRITABLESTATE = 9,
  MSG_RTC_DESTROYVOICECHANNEL = 10,
  MSG_RTC_DESTROYVIDEOCHANNEL = 11,
  MSG_RTC_SENDLOCALDESCRIPTION = 12,
  MSG_RTC_REMOVESTREAM = 13,
  MSG_RTC_REMOVEALLSTREAMS = 14,
  MSG_RTC_ENABLEALLSTREAMS = 15,
  MSG_RTC_SETSESSIONERROR = 16,
};

struct CreateChannelParams : public talk_base::MessageData {
  CreateChannelParams(const std::string& content_name, bool rtcp,
                      cricket::VoiceChannel* voice_channel)
      : content_name(content_name),
        rtcp(rtcp),
        voice_channel(voice_channel),
        video_channel(NULL) {}

  std::string content_name;
  bool rtcp;
  cricket::VoiceChannel* voice_channel;
  cricket::VideoChannel* video_channel;
};

struct SetStateParams : public talk_base::MessageData {
  SetStateParams(int state)
    : state(state) {}
  int state;
  bool result;
};

struct CaptureParams : public talk_base::MessageData {
  explicit CaptureParams(bool c) : capture(c), result(CR_FAILURE) {}

  bool capture;
  CaptureResult result;
};

struct ExternalRenderParams : public talk_base::MessageData {
  ExternalRenderParams(const std::string& stream_id,
                       ExternalRenderer* external_renderer)
      : stream_id(stream_id),
        external_renderer(external_renderer),
        result(false) {}

  const std::string stream_id;
  ExternalRenderer* external_renderer;
  bool result;
};

struct CricketRenderParams : public talk_base::MessageData {
  CricketRenderParams(const std::string& stream_id,
                      cricket::VideoRenderer* renderer)
      : stream_id(stream_id),
        renderer(renderer),
        result(false) {}

  const std::string stream_id;
  cricket::VideoRenderer* renderer;
  bool result;
};

struct ChannelEnableParams : public talk_base::MessageData {
  ChannelEnableParams(cricket::BaseChannel* channel, bool enable)
      : channel(channel), enable(enable) {}

  cricket::BaseChannel* channel;
  bool enable;
};

static const int kAudioMonitorPollFrequency = 100;
static const int kMonitorPollFrequency = 1000;

// We allow 30 seconds to establish a connection; beyond that we consider
// it an error
static const int kCallSetupTimeout = 30 * 1000;
// A loss of connectivity is probably due to the Internet connection going
// down, and it might take a while to come back on wireless networks, so we
// use a longer timeout for that.
static const int kCallLostTimeout = 60 * 1000;
static const uint32 kCandidateTimeoutId = 101;

typedef std::vector<StreamInfo*> StreamMap; // not really a map (vector)

WebRTCSessionImpl::WebRTCSessionImpl(
    const std::string& id,
    const std::string& direction,
    cricket::PortAllocator* allocator,
    cricket::ChannelManager* channelmgr,
    PeerConnection* connection,
    talk_base::Thread* signaling_thread)
  : WebRTCSession(id, direction, allocator, connection, signaling_thread),
    channel_manager_(channelmgr),
    all_writable_(false),
    muted_(false),
    camera_muted_(false),
    setup_timeout_(kCallSetupTimeout),
    signal_initiated_(false) {
}

WebRTCSessionImpl::~WebRTCSessionImpl() {
  if (state_ != STATE_RECEIVEDTERMINATE) {
    Terminate();
  }
}

bool WebRTCSessionImpl::CreateP2PTransportChannel(const std::string& stream_id,
                                                  bool video) {
  PC_Transport_Impl* transport = new PC_Transport_Impl(this);
  ASSERT(transport != NULL);
  const std::string name = ((video) ? "video_rtp" : "rtp");
  if (!transport->Init(name)) {
    delete transport;
    return false;
  }

  ASSERT(transport_channels_.find(name) == transport_channels_.end());
  transport_channels_[name] = transport;

  StreamInfo* stream_info = new StreamInfo(stream_id);
  stream_info->transport = transport;
  stream_info->video = video;
  streams_.push_back(stream_info);

  return true;
}

bool WebRTCSessionImpl::CreateVoiceChannel(const std::string& stream_id) {
  this->SignalVoiceChannel.connect(
      this, &WebRTCSessionImpl::OnVoiceChannelCreated);

  signaling_thread_->Post(this, MSG_RTC_CREATEAUDIOCHANNEL,
                          new CreateChannelParams(stream_id, true, NULL));
  return true;
}

cricket::VoiceChannel* WebRTCSessionImpl::CreateVoiceChannel_w(
    const std::string& content_name,
    bool rtcp) {
  cricket::VoiceChannel* voice_channel = channel_manager_->CreateVoiceChannel(
      this, content_name, rtcp);
  return voice_channel;
}

void WebRTCSessionImpl::OnVoiceChannelCreated(
    cricket::VoiceChannel* voice_channel,
    std::string& stream_id) {
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* stream_info = (*iter);
    if (stream_info->stream_id.compare(stream_id) == 0) {
      ASSERT(stream_info->channel == NULL);
      stream_info->channel = voice_channel;
      stream_info->media_channel =
          voice_channel->media_channel()->GetMediaChannelId();
      if (incoming()) {
        // change stream id to audio-<media_channel>
        // ^^ - The code that does this has been disabled because
        // it causes us to not be able to find the stream by name later.
        // Instead, we could store the channel_id as an int member with
        // stream_info?
        streams_.erase(iter);
#if 0
        stream_info->stream_id.append("-");
        stream_info->stream_id.append(
            talk_base::ToString(stream_info->media_channel));
#endif
        streams_.push_back(stream_info);
        connection()->OnAddStream(
            stream_info->stream_id, stream_info->media_channel, false);
      } else {
        connection()->OnRtcMediaChannelCreated(
            stream_id, stream_info->media_channel, false);
      }
      break;
    }
  }
}

bool WebRTCSessionImpl::CreateVideoChannel(const std::string& stream_id) {
  this->SignalVideoChannel.connect(
      this, &WebRTCSessionImpl::OnVideoChannelCreated);

  signaling_thread_->Post(this, MSG_RTC_CREATEVIDEOCHANNEL,
                          new CreateChannelParams(stream_id, true, NULL));
  return true;
}

cricket::VideoChannel* WebRTCSessionImpl::CreateVideoChannel_w(
    const std::string& content_name,
    bool rtcp,
    cricket::VoiceChannel* voice_channel) {
  cricket::VideoChannel* video_channel = channel_manager_->CreateVideoChannel(
      this, content_name, rtcp, voice_channel);
  return video_channel;
}

void WebRTCSessionImpl::OnVideoChannelCreated(
    cricket::VideoChannel* video_channel,
    std::string& stream_id) {
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* stream_info = (*iter);
    if (stream_info->stream_id.compare(stream_id) == 0) {
      ASSERT(stream_info->channel == NULL);
      stream_info->channel = video_channel;
      stream_info->media_channel =
          video_channel->media_channel()->GetMediaChannelId();
      if (incoming()) {
        // change stream id to video-<media_channel>
        // ^^ - The code that does this has been disabled because
        // it causes us to not be able to find the stream by name later.
        // Instead, we could store the channel_id as an int member with
        // stream_info?
        streams_.erase(iter);
#if 0
        stream_info->stream_id.append("-");
        stream_info->stream_id.append(
            talk_base::ToString(stream_info->media_channel));
#endif
        streams_.push_back(stream_info);
        connection()->OnAddStream(
            stream_info->stream_id, stream_info->media_channel, true);
      } else {
        connection()->OnRtcMediaChannelCreated(
            stream_id, stream_info->media_channel, true);
      }
      break;
    }
  }
}

bool WebRTCSessionImpl::SetVideoRenderer(const std::string& stream_id,
    cricket::VideoRenderer* renderer) {
  if(signaling_thread_ != talk_base::Thread::Current()) {
    signaling_thread_->Post(this, MSG_RTC_SETCRICKETRENDERER,
                            new CricketRenderParams(stream_id, renderer),
                            true);
    return true;
  }

  ASSERT(signaling_thread_ == talk_base::Thread::Current());

  bool ret = false;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* stream_info = (*iter);
    if (stream_info->stream_id.compare(stream_id) == 0) {
      ASSERT(stream_info->channel != NULL);
      ASSERT(stream_info->video);
      cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*>(
          stream_info->channel);
      ret = channel->SetRenderer(0, renderer);
      break;
    }
  }
  return ret;
}

bool WebRTCSessionImpl::SetVideoRenderer(const std::string& stream_id,
    ExternalRenderer* external_renderer) {
  if(signaling_thread_ != talk_base::Thread::Current()) {
    signaling_thread_->Post(this, MSG_RTC_SETEXTERNALRENDERER,
                            new ExternalRenderParams(stream_id, external_renderer),
                            true);
    return true;
  }

  ASSERT(signaling_thread_ == talk_base::Thread::Current());

  bool ret = false;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* stream_info = (*iter);
    if (stream_info->stream_id.compare(stream_id) == 0) {
      ASSERT(stream_info->channel != NULL);
      ASSERT(stream_info->video);
      cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*> (
          stream_info->channel);
      ret = channel->media_channel()->SetExternalRenderer(0, external_renderer);
      break;
    }
  }
  return ret;
}

void WebRTCSessionImpl::OnMessage(talk_base::Message* message) {
  using talk_base::TypedMessageData;
  talk_base::MessageData* data = message->pdata;
  switch(message->message_id) {
    case MSG_RTC_CREATEVIDEOCHANNEL: {
      CreateChannelParams* p = reinterpret_cast<CreateChannelParams*>(data);
      p->video_channel =
          CreateVideoChannel_w(p->content_name, p->rtcp, p->voice_channel);
      SignalVideoChannel(p->video_channel, p->content_name);
      delete p;
      break;
    }
    case MSG_RTC_CREATEAUDIOCHANNEL: {
      CreateChannelParams* p = reinterpret_cast<CreateChannelParams*>(data);
      p->voice_channel =
          CreateVoiceChannel_w(p->content_name, p->rtcp);
      SignalVoiceChannel(p->voice_channel, p->content_name);
      delete p;
      break;
    }
    case MSG_RTC_DESTROYVOICECHANNEL: {
      cricket::VoiceChannel* channel =
          reinterpret_cast<TypedMessageData<cricket::VoiceChannel*>*>(data)
              ->data();
      std::string name(channel->content_name());
      DestroyVoiceChannel_w(channel);
      delete data;
      break;
    }
    case MSG_RTC_SETSESSIONERROR: {
      int err = reinterpret_cast<TypedMessageData<int>*>(data)->data();
      BaseSession::SetError(static_cast<Error>(err));
      delete data;
      break;
    }
    case MSG_RTC_DESTROYVIDEOCHANNEL: {
      cricket::VideoChannel* channel =
          reinterpret_cast<TypedMessageData<cricket::VideoChannel*>*>(data)
              ->data();
      std::string name(channel->content_name());
      DestroyVideoChannel_w(channel);
      delete data;
      break;
    }
    case MSG_RTC_REMOVESTREAM : {
      std::string stream_id(
          reinterpret_cast<TypedMessageData<std::string>*>(data)->data());
      RemoveStream_w(stream_id);
      delete data;
      break;
    }
    case MSG_RTC_REMOVEALLSTREAMS : {
      RemoveAllStreams_w();
      delete data;
      break;
    }
    case MSG_RTC_ENABLEALLSTREAMS: {
      EnableAllStreams_w();
      delete data;
      break;
    }
    case MSG_RTC_SETSTATE : {
      SetSessionState_w();
      break;
    }
    case MSG_RTC_SETVIDEOCAPTURE : {
      CaptureParams* p = reinterpret_cast<CaptureParams*>(data);
      p->result = SetVideoCapture_w(p->capture);
      delete p;
      break;
    }
    case MSG_RTC_SETEXTERNALRENDERER : {
      ExternalRenderParams* p = reinterpret_cast<ExternalRenderParams*>(data);
      p->result = SetVideoRenderer(p->stream_id, p->external_renderer);
      delete p;
      break;
    }
    case MSG_RTC_SETCRICKETRENDERER : {
      CricketRenderParams* p = reinterpret_cast<CricketRenderParams*>(data);
      p->result = SetVideoRenderer(p->stream_id, p->renderer);
      delete p;
      break;
    }
    case MSG_RTC_CHANNELENABLE : {
      ChannelEnableParams* p = reinterpret_cast<ChannelEnableParams*>(data);
      ChannelEnable_w(p->channel, p->enable);
      delete p;
      break;
    }
    case MSG_RTC_SIGNALONWRITABLESTATE : {
      cricket::TransportChannel* channel =
          reinterpret_cast<TypedMessageData<cricket::TransportChannel*>*>(data)
              ->data();
      SignalOnWritableState_w(channel);
      delete data;
      break;
    }
    case MSG_RTC_CANDIDATETIMEOUT: {
      break;
    }
    case MSG_RTC_SENDLOCALDESCRIPTION : {
      SendLocalDescription_w();
      break;
    }
    default: {
      WebRTCSession::OnMessage(message);
    }
  }
}

bool WebRTCSessionImpl::Initiate() {
  if (streams_.empty()) {
    // nothing to initiate
    return false;
  }

  // Enable all the channels
  signaling_thread_->Post(this, MSG_RTC_ENABLEALLSTREAMS);

  SetVideoCapture(true);
  signal_initiated_ = true;

  if (local_candidates_.size() == streams_.size()) {
    SendLocalDescription();
  }
  return true;
}

void WebRTCSessionImpl::ChannelEnable(cricket::BaseChannel* channel,
                                      bool enable) {
  ASSERT(channel);
  signaling_thread_->Post(this, MSG_RTC_CHANNELENABLE,
                          new ChannelEnableParams(channel, enable), true);
}

void WebRTCSessionImpl::ChannelEnable_w(cricket::BaseChannel* channel,
                                        bool enable) {
  if (channel) {
    channel->Enable(enable);
  }
}

void WebRTCSessionImpl::SetSessionState(State state) {
  session_state_ = state;
  signaling_thread_->Post(this, MSG_RTC_SETSTATE);
}

void WebRTCSessionImpl::SetSessionState_w() {
  SetState(session_state_);
}

bool WebRTCSessionImpl::SetVideoCapture(bool capture) {
  signaling_thread_->Post(this, MSG_RTC_SETVIDEOCAPTURE,
                          new CaptureParams(capture), true);
  return true;
}

cricket::CaptureResult WebRTCSessionImpl::SetVideoCapture_w(bool capture) {
  ASSERT(signaling_thread_ == talk_base::Thread::Current());
  return channel_manager_->SetVideoCapture(capture);
}

void WebRTCSessionImpl::OnVoiceChannelError(
    cricket::VoiceChannel* voice_channel, uint32 ssrc,
    cricket::VoiceMediaChannel::Error error) {
  //report error to connection
}

void WebRTCSessionImpl::OnVideoChannelError(
    cricket::VideoChannel* video_channel, uint32 ssrc,
    cricket::VideoMediaChannel::Error error) {
  //report error to connection
}

void WebRTCSessionImpl::RemoveStream_w(const std::string& stream_id) {
  bool found = false;
  StreamMap::iterator iter;
  std::string candidate_name;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    candidate_name = sinfo->transport->name();
    if (sinfo->stream_id.compare(stream_id) == 0) {
      DisableLocalCandidate(candidate_name);
      if (!sinfo->video) {
        cricket::VoiceChannel* channel = static_cast<cricket::VoiceChannel*> (
            sinfo->channel);
        channel_manager_->DestroyVoiceChannel(channel);
      } else {
        cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*> (
            sinfo->channel);
        channel_manager_->DestroyVideoChannel(channel);
      }
      // channel and transport will be deleted in
      // DestroyVoiceChannel/DestroyVideoChannel
      found = true;
      break;
    }
  }
  if (!found) {
    LOG(LS_ERROR) << "No streams found for stream id " << stream_id;
    //TODO - trigger onError callback
  }
}

bool WebRTCSessionImpl::RemoveStream(const std::string& stream_id) {
  bool ret = true;
  if ((state_ == STATE_RECEIVEDACCEPT) ||
      (state_ == STATE_SENTACCEPT)) {

    signaling_thread_->Post(this, MSG_RTC_REMOVESTREAM,
        new talk_base::TypedMessageData<std::string>(stream_id));
  } else {
    LOG(LS_ERROR) << "Invalid session state -" << state_;
    ret = false;
  }
  return ret;
}

void WebRTCSessionImpl::DisableLocalCandidate(const std::string& name) {
  for (size_t i = 0; i < local_candidates_.size(); ++i) {
    if (local_candidates_[i].name().compare(name) == 0) {
      talk_base::SocketAddress address(local_candidates_[i].address().ip(), 0);
      local_candidates_[i].set_address(address);
    }
  }
}

void WebRTCSessionImpl::RemoveAllStreams_w() {
  // First build a list of streams to remove and then remove them.
  // The reason we do this is that if we remove the streams inside the
  // loop, a stream might get removed while we're enumerating and the iterator
  // will become invalid (and we crash).
  std::vector<std::string> streams_to_remove;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter)
    streams_to_remove.push_back((*iter)->stream_id);

  for (std::vector<std::string>::iterator i = streams_to_remove.begin();
       i != streams_to_remove.end(); ++i)
    RemoveStream_w(*i);

  SignalOnRemoveStream(this);
}

void WebRTCSessionImpl::EnableAllStreams_w() {
  StreamMap::const_iterator i;
  for (i = streams_.begin(); i != streams_.end(); ++i) {
    cricket::BaseChannel* channel = (*i)->channel;
    if (channel)
      channel->Enable(true);
  }  
}

void WebRTCSessionImpl::RemoveAllStreams() {
  signaling_thread_->Post(this, MSG_RTC_REMOVEALLSTREAMS);
}

bool WebRTCSessionImpl::HasStream(const std::string& stream_id) const {
  StreamMap::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    if (stream_id.compare(sinfo->stream_id) == 0) {
      return true;
    }
  }
  return false;
}

bool WebRTCSessionImpl::HasStream(bool video) const {
  StreamMap::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    if (sinfo->video == video) {
      return true;
    }
  }
  return false;
}

bool WebRTCSessionImpl::HasAudioStream() const {
  return HasStream(false);
}

bool WebRTCSessionImpl::HasVideoStream() const {
  return HasStream(true);
}

void WebRTCSessionImpl::OnRequestSignaling(cricket::Transport* transport) {
  transport->OnSignalingReady();
}

cricket::TransportChannel* WebRTCSessionImpl::CreateChannel(
    const std::string& content_name, const std::string& name) {

  // channel must be already present in the vector
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (content_name.compare((*iter)->stream_id) == 0) {
      StreamInfo* sinfo = (*iter);
      // if it's a incoming call, remote candidates are already received
      // in initial SignalingMessage. apply now
      if (incoming() && state_ == STATE_RECEIVEDINITIATE) {
        // process the remote candidates
        std::vector<cricket::Candidate>::iterator iter;
        for (iter = remote_candidates_.begin();
             iter != remote_candidates_.end(); ++iter) {
          std::string tname = iter->name();
          TransportChannelMap::iterator titer = transport_channels_.find(tname);
          if (titer != transport_channels_.end()) {
            titer->second->AddRemoteCandidate(*iter);
          }
        }
      }
      return sinfo->transport->GetP2PChannel();
    }
  }
  return NULL;
}

cricket::TransportChannel* WebRTCSessionImpl::GetChannel(
    const std::string& content_name, const std::string& name) {
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (content_name.compare((*iter)->stream_id) == 0) {
      PC_Transport_Impl* transport = (*iter)->transport;
      return transport->GetP2PChannel();
    }
  }
  return NULL;
}

void WebRTCSessionImpl::DestroyChannel(
    const std::string& content_name, const std::string& name) {
  bool found = false;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (content_name.compare((*iter)->stream_id) == 0) {
      PC_Transport_Impl* transport = (*iter)->transport;
      delete transport;
      (*iter)->transport = NULL;
      connection()->OnRemoveStream((*iter)->stream_id, (*iter)->media_channel,
                                   (*iter)->video);
      streams_.erase(iter);
      found = true;
      break;
    }
  }
}

void WebRTCSessionImpl::DestroyVoiceChannel_w(
    cricket::VoiceChannel* channel) {
  channel_manager_->DestroyVoiceChannel(channel);
}

void WebRTCSessionImpl::DestroyVideoChannel_w(
    cricket::VideoChannel* channel) {
  channel_manager_->DestroyVideoChannel(channel);

}

void WebRTCSessionImpl::StartTransportTimeout(int timeout) {
  talk_base::Thread::Current()->PostDelayed(timeout, this,
                                            MSG_RTC_CANDIDATETIMEOUT,
                                            NULL);
}

void WebRTCSessionImpl::ClearTransportTimeout() {
  //LOG(LS_INFO) << "ClearTransportTimeout";
  talk_base::Thread::Current()->Clear(this, MSG_RTC_CANDIDATETIMEOUT);
}

void WebRTCSessionImpl::NotifyTransportState() {
}

bool WebRTCSessionImpl::OnRemoteDescription(Json::Value& desc) {
  if ((!incoming() && state() != STATE_SENTINITIATE) ||
      (incoming() && state() != STATE_INIT)) {
    LOG(LS_WARNING) << "Invalid session state" ;
    return false;
  }

  talk_base::scoped_ptr<cricket::AudioContentDescription> audio(
      new cricket::AudioContentDescription());

  talk_base::scoped_ptr<cricket::VideoContentDescription> video(
      new cricket::VideoContentDescription());

  //TODO- get media description from Json format
  //set_remote_description();

  if (incoming()) {
    SetState(STATE_RECEIVEDINITIATE);
  }
  return true;
}

bool WebRTCSessionImpl::OnInitiateMessage(
    const cricket::SessionDescription* offer,
    std::vector<cricket::Candidate>& candidates) {
  if (!offer) {
    LOG(LS_ERROR) << "No SessionDescription from peer";
    return false;
  }

  set_remote_description(offer);
  const cricket::SessionDescription* answer = CreateAnswer(offer);

  const cricket::ContentInfo* audio_content = GetFirstAudioContent(answer);
  const cricket::ContentInfo* video_content = GetFirstVideoContent(answer);

  if (!audio_content && !video_content) {
    // no codec information of audio and video
    set_remote_description(NULL);
    delete answer;
    return false;
  }

  SetSessionState(STATE_RECEIVEDINITIATE);

  bool ret = true;
  if (audio_content) {
    ret = !HasAudioStream() &&
          CreateP2PTransportChannel(audio_content->name, false) &&
          CreateVoiceChannel(audio_content->name);
  }

  if (video_content) {
    ret = !HasVideoStream() &&
          CreateP2PTransportChannel(video_content->name, true) &&
          CreateVideoChannel(video_content->name);
  }

  delete answer;

  if (!ret) {
    LOG(LS_ERROR) << "Failed to create channel for incoming media stream";
    return false;
  }

  // Candidate processing.
  ASSERT(candidates.size());
  remote_candidates_.clear();
  remote_candidates_.insert(remote_candidates_.begin(),
                            candidates.begin(), candidates.end());
  return true;
}

bool WebRTCSessionImpl::OnRemoteDescription(
    const cricket::SessionDescription* rdesc,
    std::vector<cricket::Candidate>& candidates) {

  if (state() == STATE_SENTACCEPT || state() == STATE_RECEIVEDACCEPT) {
    return OnRemoteDescriptionUpdate(rdesc, candidates);
  }

  if ((!incoming()) && (state() != STATE_SENTINITIATE)) {
    LOG(LS_ERROR) << "invalid session state";
    return false;
  }

//  cricket::SessionDescription* answer = new cricket::SessionDescription();
//  const ContentInfo* audio_content = GetFirstAudioContent(rdesc);
//  if (audio_content) {
//    const AudioContentDescription* audio_offer =
//        static_cast<const AudioContentDescription*>(audio_content->description);
//
//    AudioContentDescription* audio_accept = new AudioContentDescription();
//
//
//   for (AudioCodecs::const_iterator theirs = audio_offer->codecs().begin();
//          theirs != audio_offer->codecs().end(); ++theirs) {
//          audio_accept->AddCodec(*theirs);
//   }
//    audio_accept->SortCodecs();
//    answer->AddContent(audio_content->name, audio_content->type, audio_accept);
//  }
//
//  const ContentInfo* video_content = GetFirstVideoContent(rdesc);
//  if (video_content) {
//    const VideoContentDescription* video_offer =
//        static_cast<const VideoContentDescription*>(video_content->description);
//
//    VideoContentDescription* video_accept = new VideoContentDescription();
//
//    for (VideoCodecs::const_iterator theirs = video_offer->codecs().begin();
//        theirs != video_offer->codecs().end(); ++theirs) {
//        video_accept->AddCodec(*theirs);
//    }
//    video_accept->SortCodecs();
//    answer->AddContent(video_content->name, video_content->type, video_accept);
//  }

  // process the remote candidates
  remote_candidates_.clear();
  std::vector<cricket::Candidate>::iterator iter;
  for (iter = candidates.begin(); iter != candidates.end(); ++iter) {
    std::string tname = iter->name();
    TransportChannelMap::iterator titer = transport_channels_.find(tname);
    if (titer != transport_channels_.end()) {
      remote_candidates_.push_back(*iter);
      titer->second->AddRemoteCandidate(*iter);
    }
  }

  set_remote_description(rdesc);
  SetSessionState(STATE_RECEIVEDACCEPT);
  return true;
}

bool WebRTCSessionImpl::OnRemoteDescriptionUpdate(
    const cricket::SessionDescription* desc,
    std::vector<cricket::Candidate>& candidates) {
  // This will be called when session is in connected state
  // In this state session expects signaling message for any removed
  // streamed by the peer.
  // check for candidates port, if its equal to 0, remove that stream
  // and provide callback OnRemoveStream else keep as it is

  for (size_t i = 0; i < candidates.size(); ++i) {
    if (candidates[i].address().port() == 0) {
      RemoveStreamOnRequest(candidates[i]);
    }
  }
  return true;
}

void WebRTCSessionImpl::RemoveStreamOnRequest(const cricket::Candidate& candidate) {
  // 1. Get Transport corresponding to candidate name
  // 2. Get StreamInfo for the transport found in step 1
  // 3. call ChannelManager Destroy voice/video method

  TransportChannelMap::iterator iter =
      transport_channels_.find(candidate.name());
  if (iter == transport_channels_.end()) {
    return;
  }

  PC_Transport_Impl* transport = iter->second;
  std::vector<StreamInfo*>::iterator siter;
  for (siter = streams_.begin(); siter != streams_.end(); ++siter) {
    StreamInfo* stream_info = (*siter);
    if (stream_info->transport == transport) {
      if (!stream_info->video) {
        cricket::VoiceChannel* channel = static_cast<cricket::VoiceChannel*> (
            stream_info->channel);
        signaling_thread_->Post(this, MSG_RTC_DESTROYVOICECHANNEL,
            new talk_base::TypedMessageData<cricket::VoiceChannel*>(channel));
      } else {
        cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*> (
            stream_info->channel);
        signaling_thread_->Post(this, MSG_RTC_DESTROYVIDEOCHANNEL,
            new talk_base::TypedMessageData<cricket::VideoChannel*>(channel));
      }
      break;
    }
  }
}

cricket::SessionDescription* WebRTCSessionImpl::CreateOffer() {

  SessionDescription* offer = new SessionDescription();
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->video) {
      // add video codecs, if there is video stream added
      VideoContentDescription* video = new VideoContentDescription();
      std::vector<cricket::VideoCodec> video_codecs;
      channel_manager_->GetSupportedVideoCodecs(&video_codecs);
      for (VideoCodecs::const_iterator codec = video_codecs.begin();
           codec != video_codecs.end(); ++codec) {
        video->AddCodec(*codec);
      }

      video->SortCodecs();
      offer->AddContent(CN_VIDEO, NS_JINGLE_RTP, video);
    } else {
      AudioContentDescription* audio = new AudioContentDescription();

      std::vector<cricket::AudioCodec> audio_codecs;
      channel_manager_->GetSupportedAudioCodecs(&audio_codecs);
      for (AudioCodecs::const_iterator codec = audio_codecs.begin();
           codec != audio_codecs.end(); ++codec) {
        audio->AddCodec(*codec);
      }

      audio->SortCodecs();
      offer->AddContent(CN_AUDIO, NS_JINGLE_RTP, audio);
    }
  }
  return offer;
}

cricket::SessionDescription* WebRTCSessionImpl::CreateAnswer(
    const cricket::SessionDescription* offer) {
  cricket::SessionDescription* answer = new cricket::SessionDescription();

  const ContentInfo* audio_content = GetFirstAudioContent(offer);
  if (audio_content) {
    const AudioContentDescription* audio_offer =
        static_cast<const AudioContentDescription*>(audio_content->description);

    AudioContentDescription* audio_accept = new AudioContentDescription();
    AudioCodecs audio_codecs;
    channel_manager_->GetSupportedAudioCodecs(&audio_codecs);

    for (AudioCodecs::const_iterator ours = audio_codecs.begin();
        ours != audio_codecs.end(); ++ours) {
      for (AudioCodecs::const_iterator theirs = audio_offer->codecs().begin();
          theirs != audio_offer->codecs().end(); ++theirs) {
        if (ours->Matches(*theirs)) {
          cricket::AudioCodec negotiated(*ours);
          negotiated.id = theirs->id;
          audio_accept->AddCodec(negotiated);
        }
      }
    }
    audio_accept->SortCodecs();
    answer->AddContent(audio_content->name, audio_content->type, audio_accept);
  }

  const ContentInfo* video_content = GetFirstVideoContent(offer);
  if (video_content) {
    const VideoContentDescription* video_offer =
        static_cast<const VideoContentDescription*>(video_content->description);

    VideoContentDescription* video_accept = new VideoContentDescription();
    VideoCodecs video_codecs;
    channel_manager_->GetSupportedVideoCodecs(&video_codecs);

    for (VideoCodecs::const_iterator ours = video_codecs.begin();
        ours != video_codecs.end(); ++ours) {
      for (VideoCodecs::const_iterator theirs = video_offer->codecs().begin();
          theirs != video_offer->codecs().end(); ++theirs) {
        if (ours->Matches(*theirs)) {
          cricket::VideoCodec negotiated(*ours);
          negotiated.id = theirs->id;
          video_accept->AddCodec(negotiated);
        }
      }
    }
    video_accept->SortCodecs();
    answer->AddContent(video_content->name, video_content->type, video_accept);
  }
  return answer;
}

void WebRTCSessionImpl::OnMute(bool mute) {
}

void WebRTCSessionImpl::OnCameraMute(bool mute) {
}

void WebRTCSessionImpl::SetError(Error error) {
  if (signaling_thread_->IsCurrent()) {
    BaseSession::SetError(error);
  } else {
    signaling_thread_->Post(this, MSG_RTC_SETSESSIONERROR,
        new talk_base::TypedMessageData<int>(error));
  }
}

void WebRTCSessionImpl::OnCandidateReady(const cricket::Candidate& address) {
  local_candidates_.push_back(address);

  // for now we are using only one candidate from each connection.
  // PC_Transport_Impl will discard remaining candidates from
  // P2PTransportChannel. When this function is called, if
  // local_candidates_ size is equal to streams_ ( if RTCP is disabled)
  // then send local session description

  // TODO(mallinath):  Is it correct to check the state variable here for
  // incoming sessions?
  if ((signal_initiated_ || state_ == STATE_RECEIVEDINITIATE) &&
      (local_candidates_.size() == streams_.size())) {
    SendLocalDescription();

    // On the receiving end, we haven't yet enabled the channels, so after
    // sending the local description, let's enable the channels.
    if (!signal_initiated_) {
      // Enable all the channels and then send our local description.
      signaling_thread_->Post(this, MSG_RTC_ENABLEALLSTREAMS);
    }
  }
}

void WebRTCSessionImpl::SendLocalDescription() {
  signaling_thread_->Post(this, MSG_RTC_SENDLOCALDESCRIPTION);
}

void WebRTCSessionImpl::SendLocalDescription_w() {
  cricket::SessionDescription* desc;
  if (incoming() && state_ == STATE_RECEIVEDINITIATE) {
    desc = CreateAnswer(remote_description_);
  } else {
    desc = CreateOffer();
  }
  if (desc) {
    set_local_description(desc);
    session_state_ = (incoming()) ? STATE_SENTACCEPT : STATE_SENTINITIATE;
    SetState(session_state_);
    connection()->OnLocalDescription(desc, local_candidates_);
  }
}

void WebRTCSessionImpl::SignalOnWritableState_w(
    cricket::TransportChannel* channel) {
  ASSERT(connection()->media_thread() == talk_base::Thread::Current());
  SignalWritableState(channel);
}

void WebRTCSessionImpl::OnStateChange(P2PTransportClass::State state,
                                      cricket::TransportChannel* channel) {
  if (P2PTransportClass::STATE_WRITABLE & state) {
    connection()->media_thread()->Post(
        this, MSG_RTC_SIGNALONWRITABLESTATE,
        new talk_base::TypedMessageData<cricket::TransportChannel*>(channel));
  }
}

void WebRTCSessionImpl::OnMessageReceived(const char* data, size_t data_size) {
}

} /* namespace webrtc */
