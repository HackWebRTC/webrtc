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

#include "talk/app/webrtc/webrtcsession.h"

#include <string>
#include <vector>

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

namespace webrtc {

enum {
  MSG_CANDIDATE_TIMEOUT = 101,
  MSG_WEBRTC_CREATE_TRANSPORT,
  MSG_WEBRTC_DELETE_TRANSPORT,
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

typedef std::vector<StreamInfo*> StreamMap;  // not really a map (vector)
static const char kVideoStream[] = "video_rtp";
static const char kAudioStream[] = "rtp";

const char WebRTCSession::kOutgoingDirection[] = "s";
const char WebRTCSession::kIncomingDirection[] = "r";

WebRTCSession::WebRTCSession(
    const std::string& id,
    const std::string& direction,
    cricket::PortAllocator* allocator,
    cricket::ChannelManager* channelmgr,
    talk_base::Thread* signaling_thread)
  : BaseSession(signaling_thread),
    transport_(NULL),
    channel_manager_(channelmgr),
    all_transports_writable_(false),
    muted_(false),
    camera_muted_(false),
    setup_timeout_(kCallSetupTimeout),
    signaling_thread_(signaling_thread),
    id_(id),
    incoming_(direction == kIncomingDirection),
    port_allocator_(allocator) {
  BaseSession::sid_ = id;
}

WebRTCSession::~WebRTCSession() {
  RemoveAllStreams();
  if (state_ != STATE_RECEIVEDTERMINATE) {
    Terminate();
  }
  signaling_thread_->Send(this, MSG_WEBRTC_DELETE_TRANSPORT, NULL);
}

bool WebRTCSession::Initiate() {
  signaling_thread_->Send(this, MSG_WEBRTC_CREATE_TRANSPORT, NULL);
  if (transport_ == NULL) {
    return false;
  }
  transport_->set_allow_local_ips(true);

  // start transports
  transport_->SignalRequestSignaling.connect(
      this, &WebRTCSession::OnRequestSignaling);
  transport_->SignalCandidatesReady.connect(
      this, &WebRTCSession::OnCandidatesReady);
  transport_->SignalWritableState.connect(
      this, &WebRTCSession::OnWritableState);
  // Limit the amount of time that setting up a call may take.
  StartTransportTimeout(kCallSetupTimeout);
  return true;
}

cricket::Transport* WebRTCSession::CreateTransport() {
  ASSERT(signaling_thread()->IsCurrent());
  return new cricket::P2PTransport(
      talk_base::Thread::Current(),
      channel_manager_->worker_thread(), port_allocator());
}

bool WebRTCSession::CreateVoiceChannel(const std::string& stream_id) {
  StreamInfo* stream_info = new StreamInfo(stream_id);
  stream_info->video = false;
  streams_.push_back(stream_info);

  // RTCP disabled
  cricket::VoiceChannel* voice_channel =
      channel_manager_->CreateVoiceChannel(this, stream_id, false);
  ASSERT(voice_channel != NULL);
  stream_info->channel = voice_channel;

  if (incoming()) {
    SignalAddStream(stream_id, false);
  } else {
    SignalRtcMediaChannelCreated(stream_id, false);
  }
  return true;
}

bool WebRTCSession::CreateVideoChannel(const std::string& stream_id) {
  StreamInfo* stream_info = new StreamInfo(stream_id);
  stream_info->video = true;
  streams_.push_back(stream_info);

  // RTCP disabled
  cricket::VideoChannel* video_channel =
      channel_manager_->CreateVideoChannel(this, stream_id, false, NULL);
  ASSERT(video_channel != NULL);
  stream_info->channel = video_channel;

  if (incoming()) {
    SignalAddStream(stream_id, true);
  } else {
    SignalRtcMediaChannelCreated(stream_id, true);
  }
  return true;
}


cricket::TransportChannel* WebRTCSession::CreateChannel(
    const std::string& content_name,
    const std::string& name) {
  if (!transport_) {
    return NULL;
  }
  std::string type;
  if (content_name.compare(kVideoStream) == 0) {
    type = cricket::NS_GINGLE_VIDEO;
  } else {
    type = cricket::NS_GINGLE_AUDIO;
  }
  cricket::TransportChannel* transport_channel =
      transport_->CreateChannel(name, type);
  ASSERT(transport_channel != NULL);
  transport_channels_[name] = transport_channel;

  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* stream_info = (*iter);
    if (stream_info->stream_id.compare(content_name) == 0) {
      ASSERT(!stream_info->channel);
      stream_info->transport = transport_channel;
      break;
    }
  }
  return transport_channel;
}

cricket::TransportChannel* WebRTCSession::GetChannel(
    const std::string& content_name, const std::string& name) {
  if (!transport_)
    return NULL;

  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (content_name.compare((*iter)->stream_id) == 0) {
      return (*iter)->transport;
    }
  }
  return NULL;
}

void WebRTCSession::DestroyChannel(
    const std::string& content_name, const std::string& name) {
  if (!transport_)
    return;

  transport_->DestroyChannel(name);

  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (content_name.compare((*iter)->stream_id) == 0) {
      (*iter)->transport = NULL;
      break;
    }
  }
}

void WebRTCSession::OnMessage(talk_base::Message* message) {
  switch (message->message_id) {
    case MSG_CANDIDATE_TIMEOUT:
      if (transport_->writable()) {
        // This should never happen: The timout triggered even
        // though a call was successfully set up.
        ASSERT(false);
      }
      SignalFailedCall();
      break;
    case MSG_WEBRTC_CREATE_TRANSPORT:
      transport_ = CreateTransport();
      break;
    case MSG_WEBRTC_DELETE_TRANSPORT:
      if (transport_) {
        delete transport_;
        transport_ = NULL;
      }
      break;
    default:
      cricket::BaseSession::OnMessage(message);
      break;
  }
}

bool WebRTCSession::Connect() {
  if (streams_.empty()) {
    // nothing to initiate
    return false;
  }
  // lets connect all the transport channels created before for this session
  transport_->ConnectChannels();

  // create an offer now. This is to call SetState
  // Actual offer will be send when OnCandidatesReady callback received
  cricket::SessionDescription* offer = CreateOffer();
  set_local_description(offer);
  SetState((incoming()) ? STATE_SENTACCEPT : STATE_SENTINITIATE);

  // Enable all the channels
  EnableAllStreams();
  SetVideoCapture(true);
  return true;
}

bool WebRTCSession::SetVideoRenderer(const std::string& stream_id,
                                         cricket::VideoRenderer* renderer) {
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

bool WebRTCSession::SetVideoCapture(bool capture) {
  channel_manager_->SetVideoCapture(capture);
  return true;
}

bool WebRTCSession::RemoveStream(const std::string& stream_id) {
  bool ret = false;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    if (sinfo->stream_id.compare(stream_id) == 0) {
      DisableLocalCandidate(sinfo->transport->name());
      if (!sinfo->video) {
        cricket::VoiceChannel* channel = static_cast<cricket::VoiceChannel*> (
            sinfo->channel);
        channel->Enable(false);
        channel_manager_->DestroyVoiceChannel(channel);
      } else {
        cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*> (
            sinfo->channel);
        channel->Enable(false);
        channel_manager_->DestroyVideoChannel(channel);
      }
      // channel and transport will be deleted in
      // DestroyVoiceChannel/DestroyVideoChannel
      ret = true;
      break;
    }
  }
  if (!ret) {
    LOG(LERROR) << "No streams found for stream id " << stream_id;
    // TODO(ronghuawu): trigger onError callback
  }
  return ret;
}

void WebRTCSession::DisableLocalCandidate(const std::string& name) {
  for (size_t i = 0; i < local_candidates_.size(); ++i) {
    if (local_candidates_[i].name().compare(name) == 0) {
      talk_base::SocketAddress address(local_candidates_[i].address().ip(), 0);
      local_candidates_[i].set_address(address);
    }
  }
}

void WebRTCSession::EnableAllStreams() {
  StreamMap::const_iterator i;
  for (i = streams_.begin(); i != streams_.end(); ++i) {
    cricket::BaseChannel* channel = (*i)->channel;
    if (channel)
      channel->Enable(true);
  }
}

void WebRTCSession::RemoveAllStreams() {
  // signaling_thread_->Post(this, MSG_RTC_REMOVEALLSTREAMS);
  // First build a list of streams to remove and then remove them.
  // The reason we do this is that if we remove the streams inside the
  // loop, a stream might get removed while we're enumerating and the iterator
  // will become invalid (and we crash).
  // streams_ entry will be removed from ChannelManager callback method
  // DestroyChannel
  std::vector<std::string> streams_to_remove;
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter)
    streams_to_remove.push_back((*iter)->stream_id);

  for (std::vector<std::string>::iterator i = streams_to_remove.begin();
       i != streams_to_remove.end(); ++i) {
    RemoveStream(*i);
  }

  SignalRemoveStream(this);
}

bool WebRTCSession::HasStream(const std::string& stream_id) const {
  StreamMap::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    if (stream_id.compare(sinfo->stream_id) == 0) {
      return true;
    }
  }
  return false;
}

bool WebRTCSession::HasStream(bool video) const {
  StreamMap::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    StreamInfo* sinfo = (*iter);
    if (sinfo->video == video) {
      return true;
    }
  }
  return false;
}

bool WebRTCSession::HasAudioStream() const {
  return HasStream(false);
}

bool WebRTCSession::HasVideoStream() const {
  return HasStream(true);
}

talk_base::Thread* WebRTCSession::worker_thread() {
  return channel_manager_->worker_thread();
}

void WebRTCSession::OnRequestSignaling(cricket::Transport* transport) {
  transport->OnSignalingReady();
}

void WebRTCSession::OnWritableState(cricket::Transport* transport) {
  ASSERT(transport == transport_);
  const bool all_transports_writable = transport_->writable();
  if (all_transports_writable) {
    if (all_transports_writable != all_transports_writable_) {
      signaling_thread_->Clear(this, MSG_CANDIDATE_TIMEOUT);
    } else {
      // At one point all channels were writable and we had full connectivity,
      // but then we lost it. Start the timeout again to kill the call if it
      // doesn't come back.
      StartTransportTimeout(kCallLostTimeout);
    }
    all_transports_writable_ = all_transports_writable;
  }
  NotifyTransportState();
  return;
}



void WebRTCSession::StartTransportTimeout(int timeout) {
  talk_base::Thread::Current()->PostDelayed(timeout, this,
                                            MSG_CANDIDATE_TIMEOUT,
                                            NULL);
}

void WebRTCSession::NotifyTransportState() {
}

bool WebRTCSession::OnInitiateMessage(
    cricket::SessionDescription* offer,
    const std::vector<cricket::Candidate>& candidates) {
  if (!offer) {
    LOG(LERROR) << "No SessionDescription from peer";
    return false;
  }

  talk_base::scoped_ptr<cricket::SessionDescription> answer;
  answer.reset(CreateAnswer(offer));

  const cricket::ContentInfo* audio_content = GetFirstAudioContent(
      answer.get());
  const cricket::ContentInfo* video_content = GetFirstVideoContent(
      answer.get());

  if (!audio_content && !video_content) {
    return false;
  }

  bool ret = true;
  if (audio_content) {
    ret = !HasAudioStream() &&
          CreateVoiceChannel(audio_content->name);
    if (!ret) {
      LOG(LERROR) << "Failed to create voice channel for "
                  << audio_content->name;
      return false;
    }
  }

  if (video_content) {
    ret = !HasVideoStream() &&
          CreateVideoChannel(video_content->name);
    if (!ret) {
      LOG(LERROR) << "Failed to create video channel for "
                  << video_content->name;
      return false;
    }
  }

  set_remote_description(offer);
  SetState(STATE_RECEIVEDINITIATE);

  transport_->ConnectChannels();
  EnableAllStreams();

  set_local_description(answer.release());
  SetState(STATE_SENTACCEPT);
  return true;
}

bool WebRTCSession::OnRemoteDescription(
    cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
  if (state() == STATE_SENTACCEPT ||
      state() == STATE_RECEIVEDACCEPT ||
      state() == STATE_INPROGRESS) {
    if (CheckForStreamDeleteMessage(candidates)) {
      return OnRemoteDescriptionUpdate(desc, candidates);
    } else {
      transport_->OnRemoteCandidates(candidates);
      return true;
    }
  }

  // Session description is always accepted.
  set_remote_description(desc);
  SetState(STATE_RECEIVEDACCEPT);
  // Will trigger OnWritableState() if successfull.
  transport_->OnRemoteCandidates(candidates);
  return true;
}

bool WebRTCSession::CheckForStreamDeleteMessage(
    const std::vector<cricket::Candidate>& candidates) {
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (candidates[i].address().port() == 0) {
      return true;
    }
  }
  return false;
}

bool WebRTCSession::OnRemoteDescriptionUpdate(
    const cricket::SessionDescription* desc,
    const std::vector<cricket::Candidate>& candidates) {
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

void WebRTCSession::RemoveStreamOnRequest(
    const cricket::Candidate& candidate) {
  // 1. Get Transport corresponding to candidate name
  // 2. Get StreamInfo for the transport found in step 1
  // 3. call ChannelManager Destroy voice/video method
//
  TransportChannelMap::iterator iter =
      transport_channels_.find(candidate.name());
  if (iter == transport_channels_.end()) {
    return;
  }

  cricket::TransportChannel* transport = iter->second;
  std::vector<StreamInfo*>::iterator siter;
  for (siter = streams_.begin(); siter != streams_.end(); ++siter) {
    StreamInfo* stream_info = (*siter);
    if (stream_info->transport == transport) {
      if (!stream_info->video) {
        cricket::VoiceChannel* channel = static_cast<cricket::VoiceChannel*> (
            stream_info->channel);
        channel->Enable(false);
        channel_manager_->DestroyVoiceChannel(channel);
      } else {
        cricket::VideoChannel* channel = static_cast<cricket::VideoChannel*> (
            stream_info->channel);
        channel->Enable(false);
        channel_manager_->DestroyVideoChannel(channel);
      }
      SignalRemoveStream2((*siter)->stream_id, (*siter)->video);
      streams_.erase(siter);
      break;
    }
  }
}

cricket::SessionDescription* WebRTCSession::CreateOffer() {
  cricket::SessionDescription* offer = new cricket::SessionDescription();
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->video) {
      // add video codecs, if there is video stream added
      cricket::VideoContentDescription* video =
          new cricket::VideoContentDescription();
      std::vector<cricket::VideoCodec> video_codecs;
      channel_manager_->GetSupportedVideoCodecs(&video_codecs);
      for (VideoCodecs::const_iterator codec = video_codecs.begin();
           codec != video_codecs.end(); ++codec) {
        video->AddCodec(*codec);
      }

      video->SortCodecs();
      offer->AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP, video);
    } else {
      cricket::AudioContentDescription* audio =
          new cricket::AudioContentDescription();

      std::vector<cricket::AudioCodec> audio_codecs;
      channel_manager_->GetSupportedAudioCodecs(&audio_codecs);
      for (AudioCodecs::const_iterator codec = audio_codecs.begin();
           codec != audio_codecs.end(); ++codec) {
        audio->AddCodec(*codec);
      }

      audio->SortCodecs();
      offer->AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP, audio);
    }
  }
  return offer;
}

cricket::SessionDescription* WebRTCSession::CreateAnswer(
    const cricket::SessionDescription* offer) {
  cricket::SessionDescription* answer = new cricket::SessionDescription();

  const cricket::ContentInfo* audio_content = GetFirstAudioContent(offer);
  if (audio_content) {
    const cricket::AudioContentDescription* audio_offer =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);

    cricket::AudioContentDescription* audio_accept =
        new cricket::AudioContentDescription();
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

  const cricket::ContentInfo* video_content = GetFirstVideoContent(offer);
  if (video_content) {
    const cricket::VideoContentDescription* video_offer =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);

    cricket::VideoContentDescription* video_accept =
        new cricket::VideoContentDescription();
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

void WebRTCSession::OnMute(bool mute) {
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (!(*iter)->video) {
      cricket::VoiceChannel* voice_channel =
          static_cast<cricket::VoiceChannel*>((*iter)->channel);
      ASSERT(voice_channel != NULL);
      voice_channel->Mute(mute);
    }
  }
}

void WebRTCSession::OnCameraMute(bool mute) {
  StreamMap::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->video) {
      cricket::VideoChannel* video_channel =
          static_cast<cricket::VideoChannel*>((*iter)->channel);
      ASSERT(video_channel != NULL);
      video_channel->Mute(mute);
    }
  }
}

void WebRTCSession::SetError(Error error) {
  BaseSession::SetError(error);
}

void WebRTCSession::OnCandidatesReady(
    cricket::Transport* transport,
    const std::vector<cricket::Candidate>& candidates) {
  std::vector<cricket::Candidate>::const_iterator iter;
  for (iter = candidates.begin(); iter != candidates.end(); ++iter) {
    local_candidates_.push_back(*iter);
  }
  SignalLocalDescription(local_description(), candidates);
}
} /* namespace webrtc */
