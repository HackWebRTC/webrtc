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

#include <map>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/mediastreamimpl.h"
#include "talk/app/webrtc_dev/peerconnectionsignaling.h"
#include "talk/app/webrtc_dev/streamcollectionimpl.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"

static const char kStreamLabel1[] = "local_stream_1";
static const char kAudioTrackLabel1[] = "local_audio_1";
static const char kVideoTrackLabel1[] = "local_video_1";
static const int kWaitTime = 5000;

namespace webrtc {

typedef std::map<std::string, scoped_refptr<MediaStream> > MediaStreamMap;
typedef std::pair<std::string, scoped_refptr<MediaStream> > RemotePair;

class MockMediaTrackObserver : public webrtc::Observer {
 public:
  explicit MockMediaTrackObserver(MediaStreamTrack* track) : track_(track) {
    track_state = track->state();
    track->RegisterObserver(this);
  }

  virtual void OnChanged() {
    track_state = track_->state();
  }

  webrtc::MediaStreamTrack::TrackState track_state;
 private:
  scoped_refptr<MediaStreamTrack> track_;
};

class MockMediaStreamObserver : public webrtc::Observer {
 public:
  explicit MockMediaStreamObserver(MediaStream* stream) : stream_(stream) {
    ready_state = stream->ready_state();
    stream_->RegisterObserver(this);
  }

  virtual void OnChanged() {
    ready_state = stream_->ready_state();
  }

  webrtc::MediaStream::ReadyState ready_state;
 private:
  scoped_refptr<MediaStream> stream_;
};

class MockSignalingObserver : public sigslot::has_slots<> {
 public:
  MockSignalingObserver()
      : update_session_description_counter_(0),
        remote_peer_(NULL) {
  }

  // New remote stream have been discovered.
  virtual void OnRemoteStreamAdded(MediaStream* remote_stream) {
    EXPECT_EQ(MediaStream::kLive, remote_stream->ready_state());
    remote_media_streams_.insert(RemotePair(remote_stream->label(),
                                            remote_stream));
  }

  // Remote stream is no longer available.
  virtual void OnRemoteStreamRemoved(MediaStream* remote_stream) {
    EXPECT_NE(remote_media_streams_.find(remote_stream->label()),
              remote_media_streams_.end());
    remote_media_streams_.erase(remote_stream->label());
  }

  // New answer ready to be sent.
  void OnSignalingMessage(PeerConnectionMessage* message) {
    if (remote_peer_) {
      remote_peer_->ProcessSignalingMessage(message, remote_local_collection_);
      // Process posted messages to allow the remote peer to process
      // the message.
      talk_base::Thread::Current()->ProcessMessages(1);
    }
    if (message->type() != PeerConnectionMessage::kError) {
      last_message = message;
    }
  }

  void OnUpdateSessionDescription(const cricket::SessionDescription* local,
                                  const cricket::SessionDescription* remote,
                                  const cricket::Candidates& candidates) {
    update_session_description_counter_++;
  }

  // Tell this object to answer the remote_peer.
  // remote_local_collection is the local collection the remote peer want to
  // send in an answer.
  void AnswerPeer(PeerConnectionSignaling* remote_peer,
                  StreamCollectionImpl* remote_local_collection) {
    remote_peer_ = remote_peer;
    remote_local_collection_ = remote_local_collection;
  }

  void CancelAnswerPeer() {
    remote_peer_ = NULL;
    remote_local_collection_.release();
  }

  MediaStream* RemoteStream(const std::string& label) {
    MediaStreamMap::iterator it = remote_media_streams_.find(label);
    if (it != remote_media_streams_.end())
      return it->second;
    return NULL;
  }

  virtual ~MockSignalingObserver() {}

  scoped_refptr<PeerConnectionMessage> last_message;
  int update_session_description_counter_;

 private:
  MediaStreamMap remote_media_streams_;
  scoped_refptr<StreamCollectionImpl> remote_local_collection_;
  PeerConnectionSignaling* remote_peer_;
};

class PeerConnectionSignalingTest: public testing::Test {
 protected:
  virtual void SetUp() {
    channel_manager_.reset(new cricket::ChannelManager(
        talk_base::Thread::Current()));
    EXPECT_TRUE(channel_manager_->Init());

    signaling1_.reset(new PeerConnectionSignaling(
        channel_manager_.get(), talk_base::Thread::Current()));
    observer1_.reset(new MockSignalingObserver());
    signaling1_->SignalNewPeerConnectionMessage.connect(
        observer1_.get(), &MockSignalingObserver::OnSignalingMessage);
    signaling1_->SignalUpdateSessionDescription.connect(
        observer1_.get(), &MockSignalingObserver::OnUpdateSessionDescription);
    signaling1_->SignalRemoteStreamAdded.connect(
        observer1_.get(), &MockSignalingObserver::OnRemoteStreamAdded);
    signaling1_->SignalRemoteStreamRemoved.connect(
        observer1_.get(), &MockSignalingObserver::OnRemoteStreamRemoved);

    signaling2_.reset(new PeerConnectionSignaling(
        channel_manager_.get(), talk_base::Thread::Current()));
    observer2_.reset(new MockSignalingObserver());
    signaling2_->SignalNewPeerConnectionMessage.connect(
        observer2_.get(), &MockSignalingObserver::OnSignalingMessage);
    signaling2_->SignalUpdateSessionDescription.connect(
        observer2_.get(), &MockSignalingObserver::OnUpdateSessionDescription);
    signaling2_->SignalRemoteStreamAdded.connect(
        observer2_.get(), &MockSignalingObserver::OnRemoteStreamAdded);
    signaling2_->SignalRemoteStreamRemoved.connect(
        observer2_.get(), &MockSignalingObserver::OnRemoteStreamRemoved);
  }

  cricket::Candidates candidates_;
  talk_base::scoped_ptr<MockSignalingObserver> observer1_;
  talk_base::scoped_ptr<MockSignalingObserver> observer2_;
  talk_base::scoped_ptr<PeerConnectionSignaling> signaling1_;
  talk_base::scoped_ptr<PeerConnectionSignaling> signaling2_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

TEST_F(PeerConnectionSignalingTest, SimpleOneWayCall) {
  // Create a local stream.
  std::string label(kStreamLabel1);
  scoped_refptr<LocalMediaStream> stream(CreateLocalMediaStream(label));
  MockMediaStreamObserver stream_observer1(stream);

  // Add a local audio track.
  scoped_refptr<LocalAudioTrack> audio_track(
      CreateLocalAudioTrack(kAudioTrackLabel1, NULL));
  stream->AddTrack(audio_track);
  MockMediaTrackObserver track_observer1(audio_track);

  // Peer 1 create an offer with only one audio track.
  scoped_refptr<StreamCollectionImpl> local_collection1(
      StreamCollectionImpl::Create());
  local_collection1->AddStream(stream);
  // Verify that the local stream is now initializing.
  EXPECT_EQ(MediaStream::kInitializing, stream_observer1.ready_state);
  // Verify that the audio track is now initializing.
  EXPECT_EQ(MediaStreamTrack::kInitializing, track_observer1.track_state);

  // Peer 2 only receive. Create an empty collection
  scoped_refptr<StreamCollectionImpl> local_collection2(
      StreamCollectionImpl::Create());

  // Connect all messages sent from Peer1 to be received on Peer2
  observer1_->AnswerPeer(signaling2_.get(), local_collection2);
  // Connect all messages sent from Peer2 to be received on Peer1
  observer2_->AnswerPeer(signaling1_.get(), local_collection1);

  // Peer 1 generates the offer. It is not sent since there is no
  // local candidates ready.
  signaling1_->CreateOffer(local_collection1);

  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);
  EXPECT_EQ(PeerConnectionSignaling::kInitializing, signaling1_->GetState());

  // Initialize signaling1_ by providing the candidates.
  signaling1_->Initialize(candidates_);
  EXPECT_EQ(PeerConnectionSignaling::kWaitingForAnswer,
            signaling1_->GetState());
  // Process posted messages to allow signaling_1 to send the offer.
  talk_base::Thread::Current()->ProcessMessages(1);

  // Verify that signaling_2 is still not initialized.
  // Even though it have received an offer.
  EXPECT_EQ(PeerConnectionSignaling::kInitializing, signaling2_->GetState());

  // Provide the candidates to signaling_2 and let it process the offer.
  signaling2_->Initialize(candidates_);
  talk_base::Thread::Current()->ProcessMessages(1);

  // Verify that the offer/answer have been exchanged and the state is good.
  EXPECT_EQ(PeerConnectionSignaling::kIdle, signaling1_->GetState());
  EXPECT_EQ(PeerConnectionSignaling::kIdle, signaling2_->GetState());

  // Verify that the local stream is now sending.
  EXPECT_EQ(MediaStream::kLive, stream_observer1.ready_state);
  // Verify that the local audio track is now sending.
  EXPECT_EQ(MediaStreamTrack::kLive, track_observer1.track_state);

  // Verify that PeerConnection2 is aware of the sending stream.
  EXPECT_TRUE(observer2_->RemoteStream(label) != NULL);

  // Verify that both peers have updated the session descriptions.
  EXPECT_EQ(1u, observer1_->update_session_description_counter_);
  EXPECT_EQ(1u, observer2_->update_session_description_counter_);
}

TEST_F(PeerConnectionSignalingTest, Glare) {
  // Initialize signaling1_ and signaling_2 by providing the candidates.
  signaling1_->Initialize(candidates_);
  signaling2_->Initialize(candidates_);
  // Create a local stream.
  std::string label(kStreamLabel1);
  scoped_refptr<LocalMediaStream> stream(CreateLocalMediaStream(label));

  // Add a local audio track.
  scoped_refptr<LocalAudioTrack> audio_track(
      CreateLocalAudioTrack(kAudioTrackLabel1, NULL));
  stream->AddTrack(audio_track);

  // Peer 1 create an offer with only one audio track.
  scoped_refptr<StreamCollectionImpl> local_collection1(
      StreamCollectionImpl::Create());
  local_collection1->AddStream(stream);
  signaling1_->CreateOffer(local_collection1);
  EXPECT_EQ(PeerConnectionSignaling::kWaitingForAnswer,
            signaling1_->GetState());
  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);

  // Peer 2 only receive. Create an empty collection.
  scoped_refptr<StreamCollectionImpl> local_collection2(
      StreamCollectionImpl::Create());
  // Peer 2 create an empty offer.
  signaling2_->CreateOffer(local_collection2);

  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);

  // Peer 2 sends the offer to Peer1 and Peer1 sends its offer to Peer2.
  ASSERT_TRUE(observer1_->last_message != NULL);
  ASSERT_TRUE(observer2_->last_message != NULL);
  signaling2_->ProcessSignalingMessage(observer1_->last_message,
                                       local_collection2);

  signaling1_->ProcessSignalingMessage(observer2_->last_message,
                                       local_collection1);

  EXPECT_EQ(PeerConnectionSignaling::kGlare, signaling1_->GetState());
  EXPECT_EQ(PeerConnectionSignaling::kGlare, signaling2_->GetState());

  // Make sure all messages are send between
  // the two PeerConnectionSignaling objects.
  observer1_->AnswerPeer(signaling2_.get(), local_collection2);
  observer2_->AnswerPeer(signaling1_.get(), local_collection1);

  // Process all delayed posted messages.
  talk_base::Thread::Current()->ProcessMessages(kWaitTime);

  EXPECT_EQ(PeerConnectionSignaling::kIdle, signaling1_->GetState());
  EXPECT_EQ(PeerConnectionSignaling::kIdle, signaling2_->GetState());

  // Verify that PeerConnection2 is aware of the sending stream.
  EXPECT_TRUE(observer2_->RemoteStream(label) != NULL);

  // Verify that both peers have updated the session descriptions.
  EXPECT_EQ(1u, observer1_->update_session_description_counter_);
  EXPECT_EQ(1u, observer2_->update_session_description_counter_);
}

TEST_F(PeerConnectionSignalingTest, AddRemoveStream) {
  // Initialize signaling1_ and signaling_2 by providing the candidates.
  signaling1_->Initialize(candidates_);
  signaling2_->Initialize(candidates_);
  // Create a local stream.
  std::string label(kStreamLabel1);
  scoped_refptr<LocalMediaStream> stream(CreateLocalMediaStream(label));
  MockMediaStreamObserver stream_observer1(stream);

  // Add a local audio track.
  scoped_refptr<LocalAudioTrack> audio_track(
      CreateLocalAudioTrack(kAudioTrackLabel1, NULL));
  stream->AddTrack(audio_track);
  MockMediaTrackObserver track_observer1(audio_track);
  audio_track->RegisterObserver(&track_observer1);

  // Add a local video track.
  scoped_refptr<LocalVideoTrack> video_track(
      CreateLocalVideoTrack(kAudioTrackLabel1, NULL));
  stream->AddTrack(audio_track);

  // Peer 1 create an empty collection
  scoped_refptr<StreamCollectionImpl> local_collection1(
      StreamCollectionImpl::Create());

  // Peer 2 create an empty collection
  scoped_refptr<StreamCollectionImpl> local_collection2(
      StreamCollectionImpl::Create());

  // Connect all messages sent from Peer1 to be received on Peer2
  observer1_->AnswerPeer(signaling2_.get(), local_collection2);
  // Connect all messages sent from Peer2 to be received on Peer1
  observer2_->AnswerPeer(signaling1_.get(), local_collection1);

  // Peer 1 creates an empty offer and send it to Peer2.
  signaling1_->CreateOffer(local_collection1);
  // Process posted messages.
  talk_base::Thread::Current()->ProcessMessages(1);

  // Verify that both peers have updated the session descriptions.
  EXPECT_EQ(1u, observer1_->update_session_description_counter_);
  EXPECT_EQ(1u, observer2_->update_session_description_counter_);

  // Peer2 add a stream.
  local_collection2->AddStream(stream);

  signaling2_->CreateOffer(local_collection2);
  talk_base::Thread::Current()->ProcessMessages(1);

  // Verify that the PeerConnection 2 local stream is now sending.
  EXPECT_EQ(MediaStream::kLive, stream_observer1.ready_state);
  EXPECT_EQ(MediaStreamTrack::kLive, track_observer1.track_state);

  // Verify that PeerConnection1 is aware of the sending stream.
  EXPECT_TRUE(observer1_->RemoteStream(label) != NULL);

  // Verify that both peers have updated the session descriptions.
  EXPECT_EQ(2u, observer1_->update_session_description_counter_);
  EXPECT_EQ(2u, observer2_->update_session_description_counter_);

  // Remove the stream
  local_collection2->RemoveStream(stream);

  signaling2_->CreateOffer(local_collection2);
  talk_base::Thread::Current()->ProcessMessages(1);

  // Verify that PeerConnection1 is not aware of the sending stream.
  EXPECT_TRUE(observer1_->RemoteStream(label) == NULL);

  // Verify that the PeerConnection 2 local stream is now ended.
  EXPECT_EQ(MediaStream::kEnded, stream_observer1.ready_state);
  EXPECT_EQ(MediaStreamTrack::kEnded, track_observer1.track_state);

  // Verify that both peers have updated the session descriptions.
  EXPECT_EQ(3u, observer1_->update_session_description_counter_);
  EXPECT_EQ(3u, observer2_->update_session_description_counter_);
}

}  // namespace webrtc
