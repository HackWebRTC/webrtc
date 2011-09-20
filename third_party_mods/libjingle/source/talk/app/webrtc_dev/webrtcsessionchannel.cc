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

#include "talk/app/webrtc/webrtcsessionchannel.h"

#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/webrtc_json_dev.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/transportchannel.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/codec.h"
#include "talk/session/phone/mediasessionclient.h"

namespace webrtc {

enum {
  MSG_WEBRTC_SENDSIGNAL = 1,
  MSG_WEBRTC_STATECHANGE,
};

static const char* direction_str[] = {
    "sendonly",
    "recvonly",
    "sendrecv",
    "inactive"
};

typedef std::vector<cricket::AudioCodec> AudioCodecs;
typedef std::vector<cricket::VideoCodec> VideoCodecs;

struct SendSignalMsgParams : public talk_base::MessageData {
  SendSignalMsgParams(const std::vector<cricket::Candidate> candidates)
      : candidates_(candidates) {
  }
  std::vector<cricket::Candidate> candidates_;
};
// TODO(mallinath) - Handling of RTCP packets when remote end point doesn't
// support RTCP muxing.

WebRtcSessionChannel::WebRtcSessionChannel(MediaStreamTrack* track,
                                           cricket::ChannelManager* cmgr,
                                           talk_base::Thread* signal_thread)
    : video_(false),
      transport_channel_name_(),
      enabled_(false),
      media_channel_(NULL),
      media_stream_track_(track),
      channel_manager_(cmgr),
      direction_(SD_SENDRECV),
      signaling_thread_(signal_thread),
      state_(STATE_INIT) {
  if (track->kind().compare(kVideoTrackKind) == 0) {
    video_ = true;
  }
  // TODO(mallinath) Register "this" object with track to get OnChanged event.
}

WebRtcSessionChannel::~WebRtcSessionChannel() {
}

void WebRtcSessionChannel::OnChanged() {
  enabled_ = !enabled_;
  media_channel_->Enable(enabled_);
}

bool WebRtcSessionChannel::Initialize(cricket::BaseSession* session) {
  // By default RTCP muxing is enabled on, rtcp flag is set to false
  // on cricket::BaseChannel.
  if (video_) {
    media_channel_.reset(channel_manager_->CreateVideoChannel(
        session, media_stream_track_->label(), false, NULL));
    transport_channel_name_ = "video_rtp";
  } else {
    media_channel_.reset(channel_manager_->CreateVoiceChannel(
        session, media_stream_track_->label(), false));
    transport_channel_name_ = "rtp";
  }
  ASSERT(!media_channel_.get());
  return true;
}

bool WebRtcSessionChannel::EnableMediaChannel(bool enable) {
  enabled_ = enable;
  return media_channel_->Enable(enable);
}

cricket::SessionDescription* WebRtcSessionChannel::GetChannelMediaDesc() {
  cricket::SessionDescription* sdp =
      new cricket::SessionDescription();
  if (video_) {
    cricket::VideoContentDescription* video =
        new cricket::VideoContentDescription();
    std::vector<cricket::VideoCodec> video_codecs;
    channel_manager_->GetSupportedVideoCodecs(&video_codecs);
    for (VideoCodecs::const_iterator codec = video_codecs.begin();
         codec != video_codecs.end(); ++codec) {
      video->AddCodec(*codec);
    }
    video->SortCodecs();
    // Enable RTCP muxing with RTP port
    video->set_rtcp_mux(true);
    sdp->AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP, video);
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
    // Enable RTCP muxing with RTP port
    audio->set_rtcp_mux(true);
    sdp->AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP, audio);
  }
  return sdp;
}

void WebRtcSessionChannel::SendSignalingMessage(
    const std::vector<cricket::Candidate>& candidates) {
  SendSignalMsgParams* msg_param = new SendSignalMsgParams(candidates);
  signaling_thread_->Post(this, MSG_WEBRTC_SENDSIGNAL, msg_param);
}

void WebRtcSessionChannel::SendSignalingMessage_s(
    const std::vector<cricket::Candidate>& candidates) {
  cricket::SessionDescription* sdp = GetChannelMediaDesc();
  ASSERT(sdp);
  std::string signaling_message;
  if (GetSignalingMessage(sdp,
                          candidates,
                          video_,
                          media_stream_track_->label(),
                          direction_str[direction_],
                          &signaling_message)) {
    set_local_description(sdp);
    SignalJSONMessageReady(this, signaling_message);
    if (state_ == STATE_INIT) {
      SetState(STATE_SENTINITIATE);
    } else {
      SetState(STATE_SENDRECV);
    }
  }
  // TODO(mallinath) - Handling on error
}

void WebRtcSessionChannel::SetState(State state) {
  if (state != state) {
    state_ = state;
    signaling_thread_->Post(this, MSG_WEBRTC_STATECHANGE);
  }
}

void WebRtcSessionChannel::OnStateChange() {
  switch (state_) {
    case STATE_SENTINITIATE:
    case STATE_RECEIVING: {
      // Don't do anything yet.
      break;
    }
    case STATE_RECEIVEDINITIATE: {
      SetState(STATE_SENTACCEPT);
      break;
    }
    case STATE_SENTACCEPT: {
      if (!SetLocalMediaContent(remote_description_, cricket::CA_OFFER)) {
        LOG(LS_ERROR) << "Failure in SetLocalMediaContent with CA_OFFER";
        SignalSessionChannelError(this, ERROR_CONTENT);
        return;
      }
      SetState(STATE_RECEIVING);
      break;
    }
    case STATE_RECEIVEDACCEPT: {
      // Start sending
      if (!SetRemoteMediaContent(remote_description_, cricket::CA_ANSWER)) {
        LOG(LS_ERROR) << "Failure in SetRemoteMediaContent with CA_ANSWER";
        SignalSessionChannelError(this, ERROR_CONTENT);
        return;
      }
      SetState(STATE_SENDING);
      break;
    }
    case STATE_SENDING: {
      // Enable channel to start sending to peer
      media_channel_->Enable(true);
      break;
    }
    case STATE_SENDRECV: {
      // Start sending
      if (media_channel_->enabled() &&
          !SetLocalMediaContent(remote_description_, cricket::CA_OFFER)) {
        LOG(LS_ERROR) << "Failure in SetRemoteMediaContent with CA_ANSWER";
        SignalSessionChannelError(this, ERROR_CONTENT);
        return;
      } else {
        if (!SetRemoteMediaContent(local_description_, cricket::CA_ANSWER)) {
          LOG(LS_ERROR) << "Failure in SetLocalmediaContent with CA_ANSWER";
          SignalSessionChannelError(this, ERROR_CONTENT);
          return;
        }
        media_channel_->Enable(true);
      }
      break;
    }
    default:
      ASSERT(false);
      break;
  }
}

bool WebRtcSessionChannel::ProcessRemoteMessage(
    cricket::SessionDescription* sdp) {
  set_remote_description(sdp);
  if (state_ == STATE_SENTINITIATE) {
    SetState(STATE_RECEIVEDACCEPT);
  } else if (state_ == STATE_INIT) {
    SetState(STATE_RECEIVEDINITIATE);
  } else if (state_ == STATE_SENDING) {
    SetState(STATE_SENDRECV);
  }
  return true;
}

bool WebRtcSessionChannel::SetLocalMediaContent(
    const cricket::SessionDescription* sdp,
    cricket::ContentAction action) {
  ASSERT(!media_channel_.get());
  const cricket::MediaContentDescription* content = NULL;
  content = GetFirstContent(sdp, video_);
  if (content && !media_channel_->SetLocalContent(content, action)) {
    LOG(LS_ERROR) << "Failure in SetLocaContent";
    return false;
  }
  return true;
}

bool WebRtcSessionChannel::SetRemoteMediaContent(
    const cricket::SessionDescription* sdp,
    cricket::ContentAction action) {
  ASSERT(!media_channel_.get());
  const cricket::MediaContentDescription* content = NULL;
  content = GetFirstContent(sdp, video_);
  if (content && !media_channel_->SetRemoteContent(content, action)) {
    LOG(LS_ERROR) << "Failure in SetRemoteContent";
    return false;
  }
  return true;
}

const cricket::MediaContentDescription* WebRtcSessionChannel::GetFirstContent(
    const cricket::SessionDescription* sdp,
    bool video) {
  const cricket::ContentInfo* cinfo = NULL;
  if (video) {
    cinfo = cricket::GetFirstVideoContent(sdp);
  } else {
    cinfo = cricket::GetFirstAudioContent(sdp);
  }
  if (cinfo == NULL) {
    return NULL;
  }
  return static_cast<const cricket::MediaContentDescription*>(
      cinfo->description);
}

void WebRtcSessionChannel::DestroyMediaChannel() {
  ASSERT(media_channel_.get());
  if (video_) {
    cricket::VideoChannel* video_channel =
        static_cast<cricket::VideoChannel*> (media_channel_.get());
    channel_manager_->DestroyVideoChannel(video_channel);
  } else {
    cricket::VoiceChannel* voice_channel =
        static_cast<cricket::VoiceChannel*> (media_channel_.get());
    channel_manager_->DestroyVoiceChannel(voice_channel);
  }
  media_channel_.reset(NULL);
  enabled_ = false;
}

void WebRtcSessionChannel::OnMessage(talk_base::Message* message) {
  talk_base::MessageData* data = message->pdata;
  switch (message->message_id) {
    case MSG_WEBRTC_SENDSIGNAL: {
      SendSignalMsgParams* p = static_cast<SendSignalMsgParams*>(data);
      SendSignalingMessage_s(p->candidates_);
      delete p;
      break;
    }
    case MSG_WEBRTC_STATECHANGE: {
      OnStateChange();
      break;
    }
    default : {
      ASSERT(false);
      break;
    }
  }
}

}  // namespace webrtc
