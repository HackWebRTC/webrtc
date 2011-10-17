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

#include "talk/app/webrtc_dev/peerconnectionsignaling.h"

#include <utility>

#include "talk/app/webrtc_dev/mediastreamtrackproxy.h"
#include "talk/app/webrtc_dev/sessiondescriptionprovider.h"
#include "talk/base/helpers.h"
#include "talk/base/messagequeue.h"
#include "talk/session/phone/channelmanager.h"

namespace webrtc {

enum {
  MSG_SEND_QUEUED_OFFER = 1,
  MSG_GENERATE_ANSWER = 2,
};
static const int kGlareMinWaitTime = 2 * 1000;  // 2 sec
static const int kGlareWaitIntervall = 1 * 1000;  // 1 sec

// Verifies that a SessionDescription contains as least one valid media content
// and a valid codec.
static bool VerifyAnswer(const cricket::SessionDescription* answer_desc) {
  // We need to verify that at least one media content with
  // a codec is available.
  const cricket::ContentInfo* audio_content =
      GetFirstAudioContent(answer_desc);
  if (audio_content) {
    const cricket::AudioContentDescription* audio_desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);
    if (audio_desc->codecs().size() > 0) {
      return true;
    }
  }
  const cricket::ContentInfo* video_content =
      GetFirstVideoContent(answer_desc);
  if (video_content) {
    const cricket::VideoContentDescription* video_desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);
    if (video_desc->codecs().size() > 0) {
      return true;
    }
  }
  return false;
}

PeerConnectionSignaling::PeerConnectionSignaling(
    talk_base::Thread* signaling_thread,
    SessionDescriptionProvider* provider)
    : signaling_thread_(signaling_thread),
      provider_(provider),
      state_(kInitializing),
      ssrc_counter_(0) {
}

PeerConnectionSignaling::~PeerConnectionSignaling() {
  // Cleanup the queued_received_offer_ if this object is
  // deleted before the offer can be processed.
  // That means we have parsed an offer and created the remote
  // session description but we have not transfered the ownership
  // to the provider yet.
  if (queued_received_offer_.first) {
    const cricket::SessionDescription* remote_desc =
        queued_received_offer_.first->desc();
    delete remote_desc;
    delete queued_received_offer_.first;
  }
}

void PeerConnectionSignaling::OnCandidatesReady(
    const cricket::Candidates& candidates) {
  ASSERT(state_ == kInitializing);
  if (state_ != kInitializing)
    return;
  // Store the candidates.
  candidates_ = candidates;
  // If we have a queued remote offer we need to handle this first.
  if (queued_received_offer_.first != NULL) {
    state_ = kIdle;
    signaling_thread_->Post(this, MSG_GENERATE_ANSWER);
  } else if (queued_offers_.size() >0) {
    // Else if we have local queued offers.
    state_ = PeerConnectionSignaling::kWaitingForAnswer;
    signaling_thread_->Post(this, MSG_SEND_QUEUED_OFFER);
  } else {
    state_ = kIdle;
  }
}

void PeerConnectionSignaling::ProcessSignalingMessage(
    const std::string& message,
    StreamCollection* local_streams) {
  ASSERT(talk_base::Thread::Current() == signaling_thread_);

  talk_base::scoped_ptr<PeerConnectionMessage> signaling_message(
      PeerConnectionMessage::Create(message));
  if (!signaling_message.get()) {
    signaling_message.reset(PeerConnectionMessage::CreateErrorMessage(
        PeerConnectionMessage::kParseError));
    SignalNewPeerConnectionMessage(signaling_message->Serialize());
  }

  switch (signaling_message->type()) {
    case PeerConnectionMessage::kOffer: {
      queued_received_offer_ =
          RemoteOfferPair(signaling_message.release(), local_streams);
      // If we are still Initializing we need to wait before we can handle
       // the offer. Queue it and handle it when the state change.
      if (state_ == kInitializing) {
        break;
      }
      // Don't handle offers when we are waiting for an answer.
      if (state_ == kWaitingForAnswer) {
        state_ = kGlare;
        // Resends our last offer in 2 to 3s.
        const int timeout = kGlareMinWaitTime +
            talk_base::CreateRandomId() % kGlareWaitIntervall;
        signaling_thread_->PostDelayed(
            timeout, this, MSG_SEND_QUEUED_OFFER, NULL);
        talk_base::scoped_ptr<PeerConnectionMessage> msg(
            PeerConnectionMessage::CreateErrorMessage(
                PeerConnectionMessage::kWrongState));
        SignalNewPeerConnectionMessage(msg->Serialize());
        break;
      }
      if (state_ == kGlare) {
        state_ = kIdle;
      }
      // Clear the MSG_SEND_QUEUED_OFFER we posted delayed.
      signaling_thread_->Clear(this, MSG_SEND_QUEUED_OFFER);
      signaling_thread_->Post(this, MSG_GENERATE_ANSWER);
      break;
    }
    case PeerConnectionMessage::kAnswer: {
      ASSERT(state_ != PeerConnectionSignaling::kIdle);
      if (state_ == PeerConnectionSignaling::kIdle)
        return;
      const cricket::SessionDescription* remote_desc =
          provider_->SetRemoteSessionDescription(
              signaling_message->desc(),
              signaling_message->candidates());
      provider_->NegotiationDone();
      UpdateRemoteStreams(remote_desc);
      scoped_refptr<StreamCollection> streams(queued_offers_.front());
      queued_offers_.pop_front();
      UpdateSendingLocalStreams(remote_desc, streams);
      // Check if we have more offers waiting in the queue.
      if (queued_offers_.size() > 0) {
        // Send the next offer.
        signaling_thread_->Post(this, MSG_SEND_QUEUED_OFFER);
      } else {
        state_ = PeerConnectionSignaling::kIdle;
      }
      break;
    }
    case PeerConnectionMessage::kError: {
      if (signaling_message->error() != PeerConnectionMessage::kWrongState) {
        SignalErrorMessageReceived(signaling_message->error());
         // An error have occurred that we can't do anything about.
        // Reset the state and wait for user action.
        queued_offers_.clear();
        state_ = kIdle;
      }
      break;
    }
  }
}

void PeerConnectionSignaling::CreateOffer(StreamCollection* local_streams) {
  ASSERT(talk_base::Thread::Current() == signaling_thread_);
  queued_offers_.push_back(local_streams);
  if (state_ == kIdle) {
    // Check if we can sent a new offer.
    // Only one offer is allowed at the time.
    state_ = PeerConnectionSignaling::kWaitingForAnswer;
    signaling_thread_->Post(this, MSG_SEND_QUEUED_OFFER);
  }
}

// Implement talk_base::MessageHandler.
void PeerConnectionSignaling::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_SEND_QUEUED_OFFER:
      CreateOffer_s();
      break;
    case MSG_GENERATE_ANSWER:
      CreateAnswer_s();
      break;
  }
}

void PeerConnectionSignaling::CreateOffer_s() {
  ASSERT(queued_offers_.size() > 0);
  scoped_refptr<StreamCollection> local_streams(queued_offers_.front());
  cricket::MediaSessionOptions options;
  InitMediaSessionOptions(&options, local_streams);

  const cricket::SessionDescription* local_desc =
      provider_->ProvideOffer(options);

  talk_base::scoped_ptr<PeerConnectionMessage> offer_message(
      PeerConnectionMessage::Create(PeerConnectionMessage::kOffer,
                                    local_desc,
                                    candidates_));
  SignalNewPeerConnectionMessage(offer_message->Serialize());
}

PeerConnectionSignaling::State PeerConnectionSignaling::GetState() {
  return state_;
}

void PeerConnectionSignaling::CreateAnswer_s() {
  talk_base::scoped_ptr<PeerConnectionMessage> message(
      queued_received_offer_.first);
  queued_received_offer_.first = NULL;
  scoped_refptr<StreamCollection> local_streams(
      queued_received_offer_.second.release());

  // Reset all pending offers. Instead, send the new streams in the answer.
  signaling_thread_->Clear(this, MSG_SEND_QUEUED_OFFER, NULL);
  queued_offers_.clear();

  // Let the provider know about the remote offer.
  // The provider takes ownership and return a pointer for us to use.
  const cricket::SessionDescription* remote_desc =
      provider_->SetRemoteSessionDescription(message->desc(),
                                             message->candidates());

  // Create a MediaSessionOptions object with the sources we want to send.
  cricket::MediaSessionOptions options;
  InitMediaSessionOptions(&options, local_streams);
  // Create an local session description based on this.
  const cricket::SessionDescription* local_desc =
      provider_->ProvideAnswer(options);

  talk_base::scoped_ptr<PeerConnectionMessage> answer_message;
  if (!VerifyAnswer(local_desc)) {
    answer_message.reset(PeerConnectionMessage::CreateErrorMessage(
        PeerConnectionMessage::kOfferNotAcceptable));

    // Signal that the new answer is ready to be sent.
    SignalNewPeerConnectionMessage(answer_message->Serialize());
    return;
  }

  answer_message.reset(PeerConnectionMessage::Create(
      PeerConnectionMessage::kAnswer, local_desc, candidates_));

  // Let the provider know the negotiation is done.
  provider_->NegotiationDone();

  SignalNewPeerConnectionMessage(answer_message->Serialize());

  UpdateRemoteStreams(message->desc());

  // Update the state of the local streams.
  UpdateSendingLocalStreams(answer_message->desc(), local_streams);
}

// Fills a MediaSessionOptions struct with the MediaTracks we want to sent given
// the local MediaStreams.
// MediaSessionOptions contains the ssrc of the media track, the cname
// corresponding to the MediaStream and a label of the track.
void PeerConnectionSignaling::InitMediaSessionOptions(
    cricket::MediaSessionOptions* options,
    StreamCollection* local_streams) {
  // In order to be able to receive video,
  // the is_video should always be true even if there are not video tracks.
  options->is_video = true;
  for (size_t i = 0; i < local_streams->count(); ++i) {
    MediaStreamInterface* stream = local_streams->at(i);
    scoped_refptr<MediaStreamTrackListInterface> tracks = stream->tracks();

    // For each track in the stream, add it to the MediaSessionOptions.
    for (size_t j = 0; j < tracks->count(); ++j) {
      scoped_refptr<MediaStreamTrackInterface> track = tracks->at(j);
      if (MediaStreamTrackInterface::kAudio == track->type()) {
        // TODO(perkj): Better ssrc?
        // Does talk_base::CreateRandomNonZeroId() generate unique id?
        if (track->ssrc() == 0)
          track->set_ssrc(++ssrc_counter_);
        options->audio_sources.push_back(cricket::SourceParam(track->ssrc(),
                                                              track->label(),
                                                              stream->label()));
      }
      if (MediaStreamTrackInterface::kVideo == track->type()) {
        if (track->ssrc() == 0)
          track->set_ssrc(++ssrc_counter_);  // TODO(perkj): Better ssrc?
        options->video_sources.push_back(cricket::SourceParam(track->ssrc(),
                                                              track->label(),
                                                              stream->label()));
      }
    }
  }
}

// Updates or Creates remote MediaStream objects given a
// remote SessionDesription.
// If the remote SessionDesription contain new remote MediaStreams
// SignalRemoteStreamAdded is triggered. If a remote MediaStream is missing from
// the remote SessionDescription SignalRemoteStreamRemoved is triggered.
void PeerConnectionSignaling::UpdateRemoteStreams(
    const cricket::SessionDescription* remote_desc) {
  RemoteStreamMap current_streams;
  typedef std::pair<std::string, scoped_refptr<MediaStreamProxy> >
      MediaStreamPair;

  const cricket::ContentInfo* audio_content = GetFirstAudioContent(remote_desc);
  if (audio_content) {
    const cricket::AudioContentDescription* audio_desc =
        static_cast<const cricket::AudioContentDescription*>(
            audio_content->description);

    for (cricket::Sources::const_iterator it = audio_desc->sources().begin();
         it != audio_desc->sources().end();
         ++it) {
      RemoteStreamMap::iterator old_streams_it =
          remote_streams_.find(it->cname);
      RemoteStreamMap::iterator new_streams_it =
          current_streams.find(it->cname);

      if (old_streams_it == remote_streams_.end()) {
        if (new_streams_it == current_streams.end()) {
          // New stream
          scoped_refptr<MediaStreamProxy> stream(
              MediaStreamProxy::Create(it->cname, signaling_thread_));
          current_streams.insert(MediaStreamPair(stream->label(), stream));
          new_streams_it = current_streams.find(it->cname);
        }
        scoped_refptr<AudioTrackInterface> track(
            AudioTrackProxy::CreateRemote(it->description, it->ssrc,
                                          signaling_thread_));
        track->set_state(MediaStreamTrackInterface::kLive);
        new_streams_it->second->AddTrack(track);

      } else {
        scoped_refptr<MediaStreamProxy> stream(old_streams_it->second);
        current_streams.insert(MediaStreamPair(stream->label(), stream));
      }
    }
  }

  const cricket::ContentInfo* video_content = GetFirstVideoContent(remote_desc);
  if (video_content) {
    const cricket::VideoContentDescription* video_desc =
        static_cast<const cricket::VideoContentDescription*>(
            video_content->description);

    for (cricket::Sources::const_iterator it = video_desc->sources().begin();
         it != video_desc->sources().end();
         ++it) {
      RemoteStreamMap::iterator old_streams_it =
          remote_streams_.find(it->cname);
      RemoteStreamMap::iterator new_streams_it =
          current_streams.find(it->cname);

      if (old_streams_it == remote_streams_.end()) {
        if (new_streams_it == current_streams.end()) {
          // New stream
          scoped_refptr<MediaStreamProxy> stream(
              MediaStreamProxy::Create(it->cname, signaling_thread_));
          current_streams.insert(MediaStreamPair(stream->label(), stream));
          new_streams_it = current_streams.find(it->cname);
        }
        scoped_refptr<VideoTrackInterface> track(
            VideoTrackProxy::CreateRemote(it->description, it->ssrc,
                                          signaling_thread_));
        new_streams_it->second->AddTrack(track);
        track->set_state(MediaStreamTrackInterface::kLive);

      } else {
        scoped_refptr<MediaStreamProxy> stream(old_streams_it->second);
        current_streams.insert(MediaStreamPair(stream->label(), stream));
      }
    }
  }

  // Iterate current_streams to find all new streams.
  // Change the state of the new stream and SignalRemoteStreamAdded.
  for (RemoteStreamMap::iterator it = current_streams.begin();
       it != current_streams.end();
       ++it) {
    scoped_refptr<MediaStreamProxy> new_stream(it->second);
    RemoteStreamMap::iterator old_streams_it =
        remote_streams_.find(new_stream->label());
    if (old_streams_it == remote_streams_.end()) {
      new_stream->set_ready_state(MediaStreamInterface::kLive);
      SignalRemoteStreamAdded(new_stream);
    }
  }

  // Iterate the old list of remote streams.
  // If a stream is not found in the new list it have been removed.
  // Change the state of the removed stream and SignalRemoteStreamRemoved.
  for (RemoteStreamMap::iterator it = remote_streams_.begin();
       it != remote_streams_.end();
       ++it) {
    scoped_refptr<MediaStreamProxy> old_stream(it->second);
    RemoteStreamMap::iterator new_streams_it =
        current_streams.find(old_stream->label());
    if (new_streams_it == current_streams.end()) {
      old_stream->set_ready_state(MediaStreamInterface::kEnded);
      scoped_refptr<MediaStreamTrackListInterface> tracklist(
          old_stream->tracks());
      for (size_t j = 0; j < tracklist->count(); ++j) {
        tracklist->at(j)->set_state(MediaStreamTrackInterface::kEnded);
      }
      SignalRemoteStreamRemoved(old_stream);
    }
  }
  // Set the remote_streams_ map to the map of MediaStreams we just created to
  // be prepared for the next offer.
  remote_streams_ = current_streams;
}

// Update the state of all local streams we have just negotiated. If the
// negotiation succeeded the state is changed to kLive, if the negotiation
// failed the state is changed to kEnded.
void PeerConnectionSignaling::UpdateSendingLocalStreams(
    const cricket::SessionDescription* answer_desc,
    StreamCollection* negotiated_streams) {
  typedef std::pair<std::string, scoped_refptr<MediaStreamInterface> >
  MediaStreamPair;
  LocalStreamMap current_local_streams;

  for (size_t i = 0; i < negotiated_streams->count(); ++i) {
    scoped_refptr<MediaStreamInterface> stream = negotiated_streams->at(i);
    scoped_refptr<MediaStreamTrackListInterface> tracklist(stream->tracks());

    bool stream_ok = false;  // A stream is ok if at least one track succeed.

    for (size_t j = 0; j < tracklist->count(); ++j) {
      scoped_refptr<MediaStreamTrackInterface> track = tracklist->at(j);
      if (MediaStreamTrackInterface::kAudio == track->type()) {
        const cricket::ContentInfo* audio_content =
            GetFirstAudioContent(answer_desc);

        if (!audio_content) {  // The remote does not accept audio.
          track->set_state(MediaStreamTrackInterface::kFailed);
          continue;
        }
        const cricket::AudioContentDescription* audio_desc =
              static_cast<const cricket::AudioContentDescription*>(
                  audio_content->description);
        // TODO(perkj): Do we need to store the codec in the track?
        if (audio_desc->codecs().size() <= 0) {
          // No common codec.
          track->set_state(MediaStreamTrackInterface::kFailed);
        }
        track->set_state(MediaStreamTrackInterface::kLive);
        stream_ok = true;
      }
      if (MediaStreamTrackInterface::kVideo == track->type()) {
        const cricket::ContentInfo* video_content =
            GetFirstVideoContent(answer_desc);

        if (!video_content) {  // The remote does not accept video.
          track->set_state(MediaStreamTrackInterface::kFailed);
          continue;
        }
        const cricket::VideoContentDescription* video_desc =
            static_cast<const cricket::VideoContentDescription*>(
                video_content->description);
        // TODO(perkj): Do we need to store the codec in the track?
        if (video_desc->codecs().size() <= 0) {
          // No common codec.
          track->set_state(MediaStreamTrackInterface::kFailed);
        }
        track->set_state(MediaStreamTrackInterface::kLive);
        stream_ok = true;
      }
    }
    if (stream_ok) {
      // We have successfully negotiated to send this stream.
      // Change the stream and store it as successfully negotiated.
      stream->set_ready_state(MediaStreamInterface::kLive);
      current_local_streams.insert(MediaStreamPair(stream->label(), stream));
    } else {
      stream->set_ready_state(MediaStreamInterface::kEnded);
    }
  }

  // Iterate the old list of remote streams.
  // If a stream is not found in the new list it have been removed.
  // Change the state of the removed stream and all its tracks to kEnded.
  for (LocalStreamMap::iterator it = local_streams_.begin();
       it != local_streams_.end();
       ++it) {
    scoped_refptr<MediaStreamInterface> old_stream(it->second);
    MediaStreamInterface* new_streams =
        negotiated_streams->find(old_stream->label());
    if (new_streams == NULL) {
      old_stream->set_ready_state(MediaStreamInterface::kEnded);
      scoped_refptr<MediaStreamTrackListInterface> tracklist(
          old_stream->tracks());
      for (size_t j = 0; j < tracklist->count(); ++j) {
        tracklist->at(j)->set_state(MediaStreamTrackInterface::kEnded);
      }
    }
  }

  // Update the local_streams_ for next update.
  local_streams_ = current_local_streams;
}

}  // namespace webrtc
