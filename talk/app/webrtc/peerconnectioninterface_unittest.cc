/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/fakeportallocatorfactory.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/test/fakedtlsidentityservice.h"
#include "talk/app/webrtc/test/mockpeerconnectionobservers.h"
#include "talk/app/webrtc/test/testsdpstrings.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/stringutils.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/sctp/sctpdataengine.h"
#include "talk/session/media/mediasession.h"

static const char kStreamLabel1[] = "local_stream_1";
static const char kStreamLabel2[] = "local_stream_2";
static const char kStreamLabel3[] = "local_stream_3";
static const int kDefaultStunPort = 3478;
static const char kStunAddressOnly[] = "stun:address";
static const char kStunInvalidPort[] = "stun:address:-1";
static const char kStunAddressPortAndMore1[] = "stun:address:port:more";
static const char kStunAddressPortAndMore2[] = "stun:address:port more";
static const char kTurnIceServerUri[] = "turn:user@turn.example.org";
static const char kTurnUsername[] = "user";
static const char kTurnPassword[] = "password";
static const char kTurnHostname[] = "turn.example.org";
static const uint32 kTimeout = 5000U;

#define MAYBE_SKIP_TEST(feature)                    \
  if (!(feature())) {                               \
    LOG(LS_INFO) << "Feature disabled... skipping"; \
    return;                                         \
  }

using talk_base::scoped_ptr;
using talk_base::scoped_refptr;
using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::DataBuffer;
using webrtc::DataChannelInterface;
using webrtc::FakeConstraints;
using webrtc::FakePortAllocatorFactory;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::MockCreateSessionDescriptionObserver;
using webrtc::MockDataChannelObserver;
using webrtc::MockSetSessionDescriptionObserver;
using webrtc::MockStatsObserver;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::PortAllocatorFactoryInterface;
using webrtc::SdpParseError;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;

namespace {

// Gets the first ssrc of given content type from the ContentInfo.
bool GetFirstSsrc(const cricket::ContentInfo* content_info, int* ssrc) {
  if (!content_info || !ssrc) {
    return false;
  }
  const cricket::MediaContentDescription* media_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info->description);
  if (!media_desc || media_desc->streams().empty()) {
    return false;
  }
  *ssrc = media_desc->streams().begin()->first_ssrc();
  return true;
}

void SetSsrcToZero(std::string* sdp) {
  const char kSdpSsrcAtribute[] = "a=ssrc:";
  const char kSdpSsrcAtributeZero[] = "a=ssrc:0";
  size_t ssrc_pos = 0;
  while ((ssrc_pos = sdp->find(kSdpSsrcAtribute, ssrc_pos)) !=
      std::string::npos) {
    size_t end_ssrc = sdp->find(" ", ssrc_pos);
    sdp->replace(ssrc_pos, end_ssrc - ssrc_pos, kSdpSsrcAtributeZero);
    ssrc_pos = end_ssrc;
  }
}

class MockPeerConnectionObserver : public PeerConnectionObserver {
 public:
  MockPeerConnectionObserver()
      : renegotiation_needed_(false),
        ice_complete_(false) {
  }
  ~MockPeerConnectionObserver() {
  }
  void SetPeerConnectionInterface(PeerConnectionInterface* pc) {
    pc_ = pc;
    if (pc) {
      state_ = pc_->signaling_state();
    }
  }
  virtual void OnError() {}
  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) {
    EXPECT_EQ(pc_->signaling_state(), new_state);
    state_ = new_state;
  }
  // TODO(bemasc): Remove this once callers transition to OnIceGatheringChange.
  virtual void OnStateChange(StateType state_changed) {
    if (pc_.get() == NULL)
      return;
    switch (state_changed) {
      case kSignalingState:
        // OnSignalingChange and OnStateChange(kSignalingState) should always
        // be called approximately simultaneously.  To ease testing, we require
        // that they always be called in that order.  This check verifies
        // that OnSignalingChange has just been called.
        EXPECT_EQ(pc_->signaling_state(), state_);
        break;
      case kIceState:
        ADD_FAILURE();
        break;
      default:
        ADD_FAILURE();
        break;
    }
  }
  virtual void OnAddStream(MediaStreamInterface* stream) {
    last_added_stream_ = stream;
  }
  virtual void OnRemoveStream(MediaStreamInterface* stream) {
    last_removed_stream_ = stream;
  }
  virtual void OnRenegotiationNeeded() {
    renegotiation_needed_ = true;
  }
  virtual void OnDataChannel(DataChannelInterface* data_channel) {
    last_datachannel_ = data_channel;
  }

  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {
    EXPECT_EQ(pc_->ice_connection_state(), new_state);
  }
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {
    EXPECT_EQ(pc_->ice_gathering_state(), new_state);
  }
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    EXPECT_NE(PeerConnectionInterface::kIceGatheringNew,
              pc_->ice_gathering_state());

    std::string sdp;
    EXPECT_TRUE(candidate->ToString(&sdp));
    EXPECT_LT(0u, sdp.size());
    last_candidate_.reset(webrtc::CreateIceCandidate(candidate->sdp_mid(),
        candidate->sdp_mline_index(), sdp, NULL));
    EXPECT_TRUE(last_candidate_.get() != NULL);
  }
  // TODO(bemasc): Remove this once callers transition to OnSignalingChange.
  virtual void OnIceComplete() {
    ice_complete_ = true;
    // OnIceGatheringChange(IceGatheringCompleted) and OnIceComplete() should
    // be called approximately simultaneously.  For ease of testing, this
    // check additionally requires that they be called in the above order.
    EXPECT_EQ(PeerConnectionInterface::kIceGatheringComplete,
      pc_->ice_gathering_state());
  }

  // Returns the label of the last added stream.
  // Empty string if no stream have been added.
  std::string GetLastAddedStreamLabel() {
    if (last_added_stream_.get())
      return last_added_stream_->label();
    return "";
  }
  std::string GetLastRemovedStreamLabel() {
    if (last_removed_stream_.get())
      return last_removed_stream_->label();
    return "";
  }

  scoped_refptr<PeerConnectionInterface> pc_;
  PeerConnectionInterface::SignalingState state_;
  scoped_ptr<IceCandidateInterface> last_candidate_;
  scoped_refptr<DataChannelInterface> last_datachannel_;
  bool renegotiation_needed_;
  bool ice_complete_;

 private:
  scoped_refptr<MediaStreamInterface> last_added_stream_;
  scoped_refptr<MediaStreamInterface> last_removed_stream_;
};

}  // namespace
class PeerConnectionInterfaceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    talk_base::InitializeSSL(NULL);
    pc_factory_ = webrtc::CreatePeerConnectionFactory(
        talk_base::Thread::Current(), talk_base::Thread::Current(), NULL, NULL,
        NULL);
    ASSERT_TRUE(pc_factory_.get() != NULL);
  }

  virtual void TearDown() {
    talk_base::CleanupSSL();
  }

  void CreatePeerConnection() {
    CreatePeerConnection("", "", NULL);
  }

  void CreatePeerConnection(webrtc::MediaConstraintsInterface* constraints) {
    CreatePeerConnection("", "", constraints);
  }

  void CreatePeerConnection(const std::string& uri,
                            const std::string& password,
                            webrtc::MediaConstraintsInterface* constraints) {
    PeerConnectionInterface::IceServer server;
    PeerConnectionInterface::IceServers servers;
    server.uri = uri;
    server.password = password;
    servers.push_back(server);

    port_allocator_factory_ = FakePortAllocatorFactory::Create();

    // TODO(jiayl): we should always pass a FakeIdentityService so that DTLS
    // is enabled by default like in Chrome (issue 2838).
    FakeIdentityService* dtls_service = NULL;
    bool dtls;
    if (FindConstraint(constraints,
                       webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                       &dtls,
                       NULL) && dtls) {
      dtls_service = new FakeIdentityService();
    }
    pc_ = pc_factory_->CreatePeerConnection(servers, constraints,
                                            port_allocator_factory_.get(),
                                            dtls_service,
                                            &observer_);
    ASSERT_TRUE(pc_.get() != NULL);
    observer_.SetPeerConnectionInterface(pc_.get());
    EXPECT_EQ(PeerConnectionInterface::kStable, observer_.state_);
  }

  void CreatePeerConnectionWithDifferentConfigurations() {
    CreatePeerConnection(kStunAddressOnly, "", NULL);
    EXPECT_EQ(1u, port_allocator_factory_->stun_configs().size());
    EXPECT_EQ(0u, port_allocator_factory_->turn_configs().size());
    EXPECT_EQ("address",
        port_allocator_factory_->stun_configs()[0].server.hostname());
    EXPECT_EQ(kDefaultStunPort,
        port_allocator_factory_->stun_configs()[0].server.port());

    CreatePeerConnection(kStunInvalidPort, "", NULL);
    EXPECT_EQ(0u, port_allocator_factory_->stun_configs().size());
    EXPECT_EQ(0u, port_allocator_factory_->turn_configs().size());

    CreatePeerConnection(kStunAddressPortAndMore1, "", NULL);
    EXPECT_EQ(0u, port_allocator_factory_->stun_configs().size());
    EXPECT_EQ(0u, port_allocator_factory_->turn_configs().size());

    CreatePeerConnection(kStunAddressPortAndMore2, "", NULL);
    EXPECT_EQ(0u, port_allocator_factory_->stun_configs().size());
    EXPECT_EQ(0u, port_allocator_factory_->turn_configs().size());

    CreatePeerConnection(kTurnIceServerUri, kTurnPassword, NULL);
    EXPECT_EQ(1u, port_allocator_factory_->stun_configs().size());
    EXPECT_EQ(1u, port_allocator_factory_->turn_configs().size());
    EXPECT_EQ(kTurnUsername,
              port_allocator_factory_->turn_configs()[0].username);
    EXPECT_EQ(kTurnPassword,
              port_allocator_factory_->turn_configs()[0].password);
    EXPECT_EQ(kTurnHostname,
              port_allocator_factory_->turn_configs()[0].server.hostname());
    EXPECT_EQ(kTurnHostname,
              port_allocator_factory_->stun_configs()[0].server.hostname());
  }

  void ReleasePeerConnection() {
    pc_ = NULL;
    observer_.SetPeerConnectionInterface(NULL);
  }

  void AddStream(const std::string& label) {
    // Create a local stream.
    scoped_refptr<MediaStreamInterface> stream(
        pc_factory_->CreateLocalMediaStream(label));
    scoped_refptr<VideoSourceInterface> video_source(
        pc_factory_->CreateVideoSource(new cricket::FakeVideoCapturer(), NULL));
    scoped_refptr<VideoTrackInterface> video_track(
        pc_factory_->CreateVideoTrack(label + "v0", video_source));
    stream->AddTrack(video_track.get());
    EXPECT_TRUE(pc_->AddStream(stream, NULL));
    EXPECT_TRUE_WAIT(observer_.renegotiation_needed_, kTimeout);
    observer_.renegotiation_needed_ = false;
  }

  void AddVoiceStream(const std::string& label) {
    // Create a local stream.
    scoped_refptr<MediaStreamInterface> stream(
        pc_factory_->CreateLocalMediaStream(label));
    scoped_refptr<AudioTrackInterface> audio_track(
        pc_factory_->CreateAudioTrack(label + "a0", NULL));
    stream->AddTrack(audio_track.get());
    EXPECT_TRUE(pc_->AddStream(stream, NULL));
    EXPECT_TRUE_WAIT(observer_.renegotiation_needed_, kTimeout);
    observer_.renegotiation_needed_ = false;
  }

  void AddAudioVideoStream(const std::string& stream_label,
                           const std::string& audio_track_label,
                           const std::string& video_track_label) {
    // Create a local stream.
    scoped_refptr<MediaStreamInterface> stream(
        pc_factory_->CreateLocalMediaStream(stream_label));
    scoped_refptr<AudioTrackInterface> audio_track(
        pc_factory_->CreateAudioTrack(
            audio_track_label, static_cast<AudioSourceInterface*>(NULL)));
    stream->AddTrack(audio_track.get());
    scoped_refptr<VideoTrackInterface> video_track(
        pc_factory_->CreateVideoTrack(video_track_label, NULL));
    stream->AddTrack(video_track.get());
    EXPECT_TRUE(pc_->AddStream(stream, NULL));
    EXPECT_TRUE_WAIT(observer_.renegotiation_needed_, kTimeout);
    observer_.renegotiation_needed_ = false;
  }

  bool DoCreateOfferAnswer(SessionDescriptionInterface** desc, bool offer) {
    talk_base::scoped_refptr<MockCreateSessionDescriptionObserver>
        observer(new talk_base::RefCountedObject<
            MockCreateSessionDescriptionObserver>());
    if (offer) {
      pc_->CreateOffer(observer, NULL);
    } else {
      pc_->CreateAnswer(observer, NULL);
    }
    EXPECT_EQ_WAIT(true, observer->called(), kTimeout);
    *desc = observer->release_desc();
    return observer->result();
  }

  bool DoCreateOffer(SessionDescriptionInterface** desc) {
    return DoCreateOfferAnswer(desc, true);
  }

  bool DoCreateAnswer(SessionDescriptionInterface** desc) {
    return DoCreateOfferAnswer(desc, false);
  }

  bool DoSetSessionDescription(SessionDescriptionInterface* desc, bool local) {
    talk_base::scoped_refptr<MockSetSessionDescriptionObserver>
        observer(new talk_base::RefCountedObject<
            MockSetSessionDescriptionObserver>());
    if (local) {
      pc_->SetLocalDescription(observer, desc);
    } else {
      pc_->SetRemoteDescription(observer, desc);
    }
    EXPECT_EQ_WAIT(true, observer->called(), kTimeout);
    return observer->result();
  }

  bool DoSetLocalDescription(SessionDescriptionInterface* desc) {
    return DoSetSessionDescription(desc, true);
  }

  bool DoSetRemoteDescription(SessionDescriptionInterface* desc) {
    return DoSetSessionDescription(desc, false);
  }

  // Calls PeerConnection::GetStats and check the return value.
  // It does not verify the values in the StatReports since a RTCP packet might
  // be required.
  bool DoGetStats(MediaStreamTrackInterface* track) {
    talk_base::scoped_refptr<MockStatsObserver> observer(
        new talk_base::RefCountedObject<MockStatsObserver>());
    if (!pc_->GetStats(
        observer, track, PeerConnectionInterface::kStatsOutputLevelStandard))
      return false;
    EXPECT_TRUE_WAIT(observer->called(), kTimeout);
    return observer->called();
  }

  void InitiateCall() {
    CreatePeerConnection();
    // Create a local stream with audio&video tracks.
    AddAudioVideoStream(kStreamLabel1, "audio_label", "video_label");
    CreateOfferReceiveAnswer();
  }

  // Verify that RTP Header extensions has been negotiated for audio and video.
  void VerifyRemoteRtpHeaderExtensions() {
    const cricket::MediaContentDescription* desc =
        cricket::GetFirstAudioContentDescription(
            pc_->remote_description()->description());
    ASSERT_TRUE(desc != NULL);
    EXPECT_GT(desc->rtp_header_extensions().size(), 0u);

    desc = cricket::GetFirstVideoContentDescription(
        pc_->remote_description()->description());
    ASSERT_TRUE(desc != NULL);
    EXPECT_GT(desc->rtp_header_extensions().size(), 0u);
  }

  void CreateOfferAsRemoteDescription() {
    talk_base::scoped_ptr<SessionDescriptionInterface> offer;
    EXPECT_TRUE(DoCreateOffer(offer.use()));
    std::string sdp;
    EXPECT_TRUE(offer->ToString(&sdp));
    SessionDescriptionInterface* remote_offer =
        webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                         sdp, NULL);
    EXPECT_TRUE(DoSetRemoteDescription(remote_offer));
    EXPECT_EQ(PeerConnectionInterface::kHaveRemoteOffer, observer_.state_);
  }

  void CreateAnswerAsLocalDescription() {
    scoped_ptr<SessionDescriptionInterface> answer;
    EXPECT_TRUE(DoCreateAnswer(answer.use()));

    // TODO(perkj): Currently SetLocalDescription fails if any parameters in an
    // audio codec change, even if the parameter has nothing to do with
    // receiving. Not all parameters are serialized to SDP.
    // Since CreatePrAnswerAsLocalDescription serialize/deserialize
    // the SessionDescription, it is necessary to do that here to in order to
    // get ReceiveOfferCreatePrAnswerAndAnswer and RenegotiateAudioOnly to pass.
    // https://code.google.com/p/webrtc/issues/detail?id=1356
    std::string sdp;
    EXPECT_TRUE(answer->ToString(&sdp));
    SessionDescriptionInterface* new_answer =
        webrtc::CreateSessionDescription(SessionDescriptionInterface::kAnswer,
                                         sdp, NULL);
    EXPECT_TRUE(DoSetLocalDescription(new_answer));
    EXPECT_EQ(PeerConnectionInterface::kStable, observer_.state_);
  }

  void CreatePrAnswerAsLocalDescription() {
    scoped_ptr<SessionDescriptionInterface> answer;
    EXPECT_TRUE(DoCreateAnswer(answer.use()));

    std::string sdp;
    EXPECT_TRUE(answer->ToString(&sdp));
    SessionDescriptionInterface* pr_answer =
        webrtc::CreateSessionDescription(SessionDescriptionInterface::kPrAnswer,
                                         sdp, NULL);
    EXPECT_TRUE(DoSetLocalDescription(pr_answer));
    EXPECT_EQ(PeerConnectionInterface::kHaveLocalPrAnswer, observer_.state_);
  }

  void CreateOfferReceiveAnswer() {
    CreateOfferAsLocalDescription();
    std::string sdp;
    EXPECT_TRUE(pc_->local_description()->ToString(&sdp));
    CreateAnswerAsRemoteDescription(sdp);
  }

  void CreateOfferAsLocalDescription() {
    talk_base::scoped_ptr<SessionDescriptionInterface> offer;
    ASSERT_TRUE(DoCreateOffer(offer.use()));
    // TODO(perkj): Currently SetLocalDescription fails if any parameters in an
    // audio codec change, even if the parameter has nothing to do with
    // receiving. Not all parameters are serialized to SDP.
    // Since CreatePrAnswerAsLocalDescription serialize/deserialize
    // the SessionDescription, it is necessary to do that here to in order to
    // get ReceiveOfferCreatePrAnswerAndAnswer and RenegotiateAudioOnly to pass.
    // https://code.google.com/p/webrtc/issues/detail?id=1356
    std::string sdp;
    EXPECT_TRUE(offer->ToString(&sdp));
    SessionDescriptionInterface* new_offer =
            webrtc::CreateSessionDescription(
                SessionDescriptionInterface::kOffer,
                sdp, NULL);

    EXPECT_TRUE(DoSetLocalDescription(new_offer));
    EXPECT_EQ(PeerConnectionInterface::kHaveLocalOffer, observer_.state_);
    // Wait for the ice_complete message, so that SDP will have candidates.
    EXPECT_TRUE_WAIT(observer_.ice_complete_, kTimeout);
  }

  void CreateAnswerAsRemoteDescription(const std::string& offer) {
    webrtc::JsepSessionDescription* answer = new webrtc::JsepSessionDescription(
        SessionDescriptionInterface::kAnswer);
    EXPECT_TRUE(answer->Initialize(offer, NULL));
    EXPECT_TRUE(DoSetRemoteDescription(answer));
    EXPECT_EQ(PeerConnectionInterface::kStable, observer_.state_);
  }

  void CreatePrAnswerAndAnswerAsRemoteDescription(const std::string& offer) {
    webrtc::JsepSessionDescription* pr_answer =
        new webrtc::JsepSessionDescription(
            SessionDescriptionInterface::kPrAnswer);
    EXPECT_TRUE(pr_answer->Initialize(offer, NULL));
    EXPECT_TRUE(DoSetRemoteDescription(pr_answer));
    EXPECT_EQ(PeerConnectionInterface::kHaveRemotePrAnswer, observer_.state_);
    webrtc::JsepSessionDescription* answer =
        new webrtc::JsepSessionDescription(
            SessionDescriptionInterface::kAnswer);
    EXPECT_TRUE(answer->Initialize(offer, NULL));
    EXPECT_TRUE(DoSetRemoteDescription(answer));
    EXPECT_EQ(PeerConnectionInterface::kStable, observer_.state_);
  }

  // Help function used for waiting until a the last signaled remote stream has
  // the same label as |stream_label|. In a few of the tests in this file we
  // answer with the same session description as we offer and thus we can
  // check if OnAddStream have been called with the same stream as we offer to
  // send.
  void WaitAndVerifyOnAddStream(const std::string& stream_label) {
    EXPECT_EQ_WAIT(stream_label, observer_.GetLastAddedStreamLabel(), kTimeout);
  }

  // Creates an offer and applies it as a local session description.
  // Creates an answer with the same SDP an the offer but removes all lines
  // that start with a:ssrc"
  void CreateOfferReceiveAnswerWithoutSsrc() {
    CreateOfferAsLocalDescription();
    std::string sdp;
    EXPECT_TRUE(pc_->local_description()->ToString(&sdp));
    SetSsrcToZero(&sdp);
    CreateAnswerAsRemoteDescription(sdp);
  }

  scoped_refptr<FakePortAllocatorFactory> port_allocator_factory_;
  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
  scoped_refptr<PeerConnectionInterface> pc_;
  MockPeerConnectionObserver observer_;
};

TEST_F(PeerConnectionInterfaceTest,
       CreatePeerConnectionWithDifferentConfigurations) {
  CreatePeerConnectionWithDifferentConfigurations();
}

TEST_F(PeerConnectionInterfaceTest, AddStreams) {
  CreatePeerConnection();
  AddStream(kStreamLabel1);
  AddVoiceStream(kStreamLabel2);
  ASSERT_EQ(2u, pc_->local_streams()->count());

  // Test we can add multiple local streams to one peerconnection.
  scoped_refptr<MediaStreamInterface> stream(
      pc_factory_->CreateLocalMediaStream(kStreamLabel3));
  scoped_refptr<AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack(
          kStreamLabel3, static_cast<AudioSourceInterface*>(NULL)));
  stream->AddTrack(audio_track.get());
  EXPECT_TRUE(pc_->AddStream(stream, NULL));
  EXPECT_EQ(3u, pc_->local_streams()->count());

  // Remove the third stream.
  pc_->RemoveStream(pc_->local_streams()->at(2));
  EXPECT_EQ(2u, pc_->local_streams()->count());

  // Remove the second stream.
  pc_->RemoveStream(pc_->local_streams()->at(1));
  EXPECT_EQ(1u, pc_->local_streams()->count());

  // Remove the first stream.
  pc_->RemoveStream(pc_->local_streams()->at(0));
  EXPECT_EQ(0u, pc_->local_streams()->count());
}

TEST_F(PeerConnectionInterfaceTest, RemoveStream) {
  CreatePeerConnection();
  AddStream(kStreamLabel1);
  ASSERT_EQ(1u, pc_->local_streams()->count());
  pc_->RemoveStream(pc_->local_streams()->at(0));
  EXPECT_EQ(0u, pc_->local_streams()->count());
}

TEST_F(PeerConnectionInterfaceTest, CreateOfferReceiveAnswer) {
  InitiateCall();
  WaitAndVerifyOnAddStream(kStreamLabel1);
  VerifyRemoteRtpHeaderExtensions();
}

TEST_F(PeerConnectionInterfaceTest, CreateOfferReceivePrAnswerAndAnswer) {
  CreatePeerConnection();
  AddStream(kStreamLabel1);
  CreateOfferAsLocalDescription();
  std::string offer;
  EXPECT_TRUE(pc_->local_description()->ToString(&offer));
  CreatePrAnswerAndAnswerAsRemoteDescription(offer);
  WaitAndVerifyOnAddStream(kStreamLabel1);
}

TEST_F(PeerConnectionInterfaceTest, ReceiveOfferCreateAnswer) {
  CreatePeerConnection();
  AddStream(kStreamLabel1);

  CreateOfferAsRemoteDescription();
  CreateAnswerAsLocalDescription();

  WaitAndVerifyOnAddStream(kStreamLabel1);
}

TEST_F(PeerConnectionInterfaceTest, ReceiveOfferCreatePrAnswerAndAnswer) {
  CreatePeerConnection();
  AddStream(kStreamLabel1);

  CreateOfferAsRemoteDescription();
  CreatePrAnswerAsLocalDescription();
  CreateAnswerAsLocalDescription();

  WaitAndVerifyOnAddStream(kStreamLabel1);
}

TEST_F(PeerConnectionInterfaceTest, Renegotiate) {
  InitiateCall();
  ASSERT_EQ(1u, pc_->remote_streams()->count());
  pc_->RemoveStream(pc_->local_streams()->at(0));
  CreateOfferReceiveAnswer();
  EXPECT_EQ(0u, pc_->remote_streams()->count());
  AddStream(kStreamLabel1);
  CreateOfferReceiveAnswer();
}

// Tests that after negotiating an audio only call, the respondent can perform a
// renegotiation that removes the audio stream.
TEST_F(PeerConnectionInterfaceTest, RenegotiateAudioOnly) {
  CreatePeerConnection();
  AddVoiceStream(kStreamLabel1);
  CreateOfferAsRemoteDescription();
  CreateAnswerAsLocalDescription();

  ASSERT_EQ(1u, pc_->remote_streams()->count());
  pc_->RemoveStream(pc_->local_streams()->at(0));
  CreateOfferReceiveAnswer();
  EXPECT_EQ(0u, pc_->remote_streams()->count());
}

// Test that candidates are generated and that we can parse our own candidates.
TEST_F(PeerConnectionInterfaceTest, IceCandidates) {
  CreatePeerConnection();

  EXPECT_FALSE(pc_->AddIceCandidate(observer_.last_candidate_.get()));
  // SetRemoteDescription takes ownership of offer.
  SessionDescriptionInterface* offer = NULL;
  AddStream(kStreamLabel1);
  EXPECT_TRUE(DoCreateOffer(&offer));
  EXPECT_TRUE(DoSetRemoteDescription(offer));

  // SetLocalDescription takes ownership of answer.
  SessionDescriptionInterface* answer = NULL;
  EXPECT_TRUE(DoCreateAnswer(&answer));
  EXPECT_TRUE(DoSetLocalDescription(answer));

  EXPECT_TRUE_WAIT(observer_.last_candidate_.get() != NULL, kTimeout);
  EXPECT_TRUE_WAIT(observer_.ice_complete_, kTimeout);

  EXPECT_TRUE(pc_->AddIceCandidate(observer_.last_candidate_.get()));
}

// Test that the CreateOffer and CreatAnswer will fail if the track labels are
// not unique.
TEST_F(PeerConnectionInterfaceTest, CreateOfferAnswerWithInvalidStream) {
  CreatePeerConnection();
  // Create a regular offer for the CreateAnswer test later.
  SessionDescriptionInterface* offer = NULL;
  EXPECT_TRUE(DoCreateOffer(&offer));
  EXPECT_TRUE(offer != NULL);
  delete offer;
  offer = NULL;

  // Create a local stream with audio&video tracks having same label.
  AddAudioVideoStream(kStreamLabel1, "track_label", "track_label");

  // Test CreateOffer
  EXPECT_FALSE(DoCreateOffer(&offer));

  // Test CreateAnswer
  SessionDescriptionInterface* answer = NULL;
  EXPECT_FALSE(DoCreateAnswer(&answer));
}

// Test that we will get different SSRCs for each tracks in the offer and answer
// we created.
TEST_F(PeerConnectionInterfaceTest, SsrcInOfferAnswer) {
  CreatePeerConnection();
  // Create a local stream with audio&video tracks having different labels.
  AddAudioVideoStream(kStreamLabel1, "audio_label", "video_label");

  // Test CreateOffer
  scoped_ptr<SessionDescriptionInterface> offer;
  EXPECT_TRUE(DoCreateOffer(offer.use()));
  int audio_ssrc = 0;
  int video_ssrc = 0;
  EXPECT_TRUE(GetFirstSsrc(GetFirstAudioContent(offer->description()),
                           &audio_ssrc));
  EXPECT_TRUE(GetFirstSsrc(GetFirstVideoContent(offer->description()),
                           &video_ssrc));
  EXPECT_NE(audio_ssrc, video_ssrc);

  // Test CreateAnswer
  EXPECT_TRUE(DoSetRemoteDescription(offer.release()));
  scoped_ptr<SessionDescriptionInterface> answer;
  EXPECT_TRUE(DoCreateAnswer(answer.use()));
  audio_ssrc = 0;
  video_ssrc = 0;
  EXPECT_TRUE(GetFirstSsrc(GetFirstAudioContent(answer->description()),
                           &audio_ssrc));
  EXPECT_TRUE(GetFirstSsrc(GetFirstVideoContent(answer->description()),
                           &video_ssrc));
  EXPECT_NE(audio_ssrc, video_ssrc);
}

// Test that we can specify a certain track that we want statistics about.
TEST_F(PeerConnectionInterfaceTest, GetStatsForSpecificTrack) {
  InitiateCall();
  ASSERT_LT(0u, pc_->remote_streams()->count());
  ASSERT_LT(0u, pc_->remote_streams()->at(0)->GetAudioTracks().size());
  scoped_refptr<MediaStreamTrackInterface> remote_audio =
      pc_->remote_streams()->at(0)->GetAudioTracks()[0];
  EXPECT_TRUE(DoGetStats(remote_audio));

  // Remove the stream. Since we are sending to our selves the local
  // and the remote stream is the same.
  pc_->RemoveStream(pc_->local_streams()->at(0));
  // Do a re-negotiation.
  CreateOfferReceiveAnswer();

  ASSERT_EQ(0u, pc_->remote_streams()->count());

  // Test that we still can get statistics for the old track. Even if it is not
  // sent any longer.
  EXPECT_TRUE(DoGetStats(remote_audio));
}

// Test that we can get stats on a video track.
TEST_F(PeerConnectionInterfaceTest, GetStatsForVideoTrack) {
  InitiateCall();
  ASSERT_LT(0u, pc_->remote_streams()->count());
  ASSERT_LT(0u, pc_->remote_streams()->at(0)->GetVideoTracks().size());
  scoped_refptr<MediaStreamTrackInterface> remote_video =
      pc_->remote_streams()->at(0)->GetVideoTracks()[0];
  EXPECT_TRUE(DoGetStats(remote_video));
}

// Test that we don't get statistics for an invalid track.
TEST_F(PeerConnectionInterfaceTest, GetStatsForInvalidTrack) {
  InitiateCall();
  scoped_refptr<AudioTrackInterface> unknown_audio_track(
      pc_factory_->CreateAudioTrack("unknown track", NULL));
  EXPECT_FALSE(DoGetStats(unknown_audio_track));
}

// This test setup two RTP data channels in loop back.
TEST_F(PeerConnectionInterfaceTest, TestDataChannel) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);
  scoped_refptr<DataChannelInterface> data1  =
      pc_->CreateDataChannel("test1", NULL);
  scoped_refptr<DataChannelInterface> data2  =
      pc_->CreateDataChannel("test2", NULL);
  ASSERT_TRUE(data1 != NULL);
  talk_base::scoped_ptr<MockDataChannelObserver> observer1(
      new MockDataChannelObserver(data1));
  talk_base::scoped_ptr<MockDataChannelObserver> observer2(
      new MockDataChannelObserver(data2));

  EXPECT_EQ(DataChannelInterface::kConnecting, data1->state());
  EXPECT_EQ(DataChannelInterface::kConnecting, data2->state());
  std::string data_to_send1 = "testing testing";
  std::string data_to_send2 = "testing something else";
  EXPECT_FALSE(data1->Send(DataBuffer(data_to_send1)));

  CreateOfferReceiveAnswer();
  EXPECT_TRUE_WAIT(observer1->IsOpen(), kTimeout);
  EXPECT_TRUE_WAIT(observer2->IsOpen(), kTimeout);

  EXPECT_EQ(DataChannelInterface::kOpen, data1->state());
  EXPECT_EQ(DataChannelInterface::kOpen, data2->state());
  EXPECT_TRUE(data1->Send(DataBuffer(data_to_send1)));
  EXPECT_TRUE(data2->Send(DataBuffer(data_to_send2)));

  EXPECT_EQ_WAIT(data_to_send1, observer1->last_message(), kTimeout);
  EXPECT_EQ_WAIT(data_to_send2, observer2->last_message(), kTimeout);

  data1->Close();
  EXPECT_EQ(DataChannelInterface::kClosing, data1->state());
  CreateOfferReceiveAnswer();
  EXPECT_FALSE(observer1->IsOpen());
  EXPECT_EQ(DataChannelInterface::kClosed, data1->state());
  EXPECT_TRUE(observer2->IsOpen());

  data_to_send2 = "testing something else again";
  EXPECT_TRUE(data2->Send(DataBuffer(data_to_send2)));

  EXPECT_EQ_WAIT(data_to_send2, observer2->last_message(), kTimeout);
}

// This test verifies that sendnig binary data over RTP data channels should
// fail.
TEST_F(PeerConnectionInterfaceTest, TestSendBinaryOnRtpDataChannel) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);
  scoped_refptr<DataChannelInterface> data1  =
      pc_->CreateDataChannel("test1", NULL);
  scoped_refptr<DataChannelInterface> data2  =
      pc_->CreateDataChannel("test2", NULL);
  ASSERT_TRUE(data1 != NULL);
  talk_base::scoped_ptr<MockDataChannelObserver> observer1(
      new MockDataChannelObserver(data1));
  talk_base::scoped_ptr<MockDataChannelObserver> observer2(
      new MockDataChannelObserver(data2));

  EXPECT_EQ(DataChannelInterface::kConnecting, data1->state());
  EXPECT_EQ(DataChannelInterface::kConnecting, data2->state());

  CreateOfferReceiveAnswer();
  EXPECT_TRUE_WAIT(observer1->IsOpen(), kTimeout);
  EXPECT_TRUE_WAIT(observer2->IsOpen(), kTimeout);

  EXPECT_EQ(DataChannelInterface::kOpen, data1->state());
  EXPECT_EQ(DataChannelInterface::kOpen, data2->state());

  talk_base::Buffer buffer("test", 4);
  EXPECT_FALSE(data1->Send(DataBuffer(buffer, true)));
}

// This test setup a RTP data channels in loop back and test that a channel is
// opened even if the remote end answer with a zero SSRC.
TEST_F(PeerConnectionInterfaceTest, TestSendOnlyDataChannel) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);
  scoped_refptr<DataChannelInterface> data1  =
      pc_->CreateDataChannel("test1", NULL);
  talk_base::scoped_ptr<MockDataChannelObserver> observer1(
      new MockDataChannelObserver(data1));

  CreateOfferReceiveAnswerWithoutSsrc();

  EXPECT_TRUE_WAIT(observer1->IsOpen(), kTimeout);

  data1->Close();
  EXPECT_EQ(DataChannelInterface::kClosing, data1->state());
  CreateOfferReceiveAnswerWithoutSsrc();
  EXPECT_EQ(DataChannelInterface::kClosed, data1->state());
  EXPECT_FALSE(observer1->IsOpen());
}

// This test that if a data channel is added in an answer a receive only channel
// channel is created.
TEST_F(PeerConnectionInterfaceTest, TestReceiveOnlyDataChannel) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);

  std::string offer_label = "offer_channel";
  scoped_refptr<DataChannelInterface> offer_channel  =
      pc_->CreateDataChannel(offer_label, NULL);

  CreateOfferAsLocalDescription();

  // Replace the data channel label in the offer and apply it as an answer.
  std::string receive_label = "answer_channel";
  std::string sdp;
  EXPECT_TRUE(pc_->local_description()->ToString(&sdp));
  talk_base::replace_substrs(offer_label.c_str(), offer_label.length(),
                             receive_label.c_str(), receive_label.length(),
                             &sdp);
  CreateAnswerAsRemoteDescription(sdp);

  // Verify that a new incoming data channel has been created and that
  // it is open but can't we written to.
  ASSERT_TRUE(observer_.last_datachannel_ != NULL);
  DataChannelInterface* received_channel = observer_.last_datachannel_;
  EXPECT_EQ(DataChannelInterface::kConnecting, received_channel->state());
  EXPECT_EQ(receive_label, received_channel->label());
  EXPECT_FALSE(received_channel->Send(DataBuffer("something")));

  // Verify that the channel we initially offered has been rejected.
  EXPECT_EQ(DataChannelInterface::kClosed, offer_channel->state());

  // Do another offer / answer exchange and verify that the data channel is
  // opened.
  CreateOfferReceiveAnswer();
  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, received_channel->state(),
                 kTimeout);
}

// This test that no data channel is returned if a reliable channel is
// requested.
// TODO(perkj): Remove this test once reliable channels are implemented.
TEST_F(PeerConnectionInterfaceTest, CreateReliableRtpDataChannelShouldFail) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);

  std::string label = "test";
  webrtc::DataChannelInit config;
  config.reliable = true;
  scoped_refptr<DataChannelInterface> channel  =
      pc_->CreateDataChannel(label, &config);
  EXPECT_TRUE(channel == NULL);
}

// This tests that a SCTP data channel is returned using different
// DataChannelInit configurations.
TEST_F(PeerConnectionInterfaceTest, CreateSctpDataChannel) {
  FakeConstraints constraints;
  constraints.SetAllowDtlsSctpDataChannels();
  CreatePeerConnection(&constraints);

  webrtc::DataChannelInit config;

  scoped_refptr<DataChannelInterface> channel =
      pc_->CreateDataChannel("1", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_TRUE(channel->reliable());

  config.ordered = false;
  channel = pc_->CreateDataChannel("2", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_TRUE(channel->reliable());

  config.ordered = true;
  config.maxRetransmits = 0;
  channel = pc_->CreateDataChannel("3", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_FALSE(channel->reliable());

  config.maxRetransmits = -1;
  config.maxRetransmitTime = 0;
  channel = pc_->CreateDataChannel("4", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_FALSE(channel->reliable());
}

// This tests that no data channel is returned if both maxRetransmits and
// maxRetransmitTime are set for SCTP data channels.
TEST_F(PeerConnectionInterfaceTest,
       CreateSctpDataChannelShouldFailForInvalidConfig) {
  FakeConstraints constraints;
  constraints.SetAllowDtlsSctpDataChannels();
  CreatePeerConnection(&constraints);

  std::string label = "test";
  webrtc::DataChannelInit config;
  config.maxRetransmits = 0;
  config.maxRetransmitTime = 0;

  scoped_refptr<DataChannelInterface> channel =
      pc_->CreateDataChannel(label, &config);
  EXPECT_TRUE(channel == NULL);
}

// The test verifies that creating a SCTP data channel with an id already in use
// or out of range should fail.
TEST_F(PeerConnectionInterfaceTest,
       CreateSctpDataChannelWithInvalidIdShouldFail) {
  FakeConstraints constraints;
  constraints.SetAllowDtlsSctpDataChannels();
  CreatePeerConnection(&constraints);

  webrtc::DataChannelInit config;
  scoped_refptr<DataChannelInterface> channel;

  config.id = 1;
  channel = pc_->CreateDataChannel("1", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_EQ(1, channel->id());

  channel = pc_->CreateDataChannel("x", &config);
  EXPECT_TRUE(channel == NULL);

  config.id = cricket::kMaxSctpSid;
  channel = pc_->CreateDataChannel("max", &config);
  EXPECT_TRUE(channel != NULL);
  EXPECT_EQ(config.id, channel->id());

  config.id = cricket::kMaxSctpSid + 1;
  channel = pc_->CreateDataChannel("x", &config);
  EXPECT_TRUE(channel == NULL);
}

// This test that a data channel closes when a PeerConnection is deleted/closed.
TEST_F(PeerConnectionInterfaceTest, DataChannelCloseWhenPeerConnectionClose) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);

  scoped_refptr<DataChannelInterface> data1  =
      pc_->CreateDataChannel("test1", NULL);
  scoped_refptr<DataChannelInterface> data2  =
      pc_->CreateDataChannel("test2", NULL);
  ASSERT_TRUE(data1 != NULL);
  talk_base::scoped_ptr<MockDataChannelObserver> observer1(
      new MockDataChannelObserver(data1));
  talk_base::scoped_ptr<MockDataChannelObserver> observer2(
      new MockDataChannelObserver(data2));

  CreateOfferReceiveAnswer();
  EXPECT_TRUE_WAIT(observer1->IsOpen(), kTimeout);
  EXPECT_TRUE_WAIT(observer2->IsOpen(), kTimeout);

  ReleasePeerConnection();
  EXPECT_EQ(DataChannelInterface::kClosed, data1->state());
  EXPECT_EQ(DataChannelInterface::kClosed, data2->state());
}

// This test that data channels can be rejected in an answer.
TEST_F(PeerConnectionInterfaceTest, TestRejectDataChannelInAnswer) {
  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  CreatePeerConnection(&constraints);

  scoped_refptr<DataChannelInterface> offer_channel(
      pc_->CreateDataChannel("offer_channel", NULL));

  CreateOfferAsLocalDescription();

  // Create an answer where the m-line for data channels are rejected.
  std::string sdp;
  EXPECT_TRUE(pc_->local_description()->ToString(&sdp));
  webrtc::JsepSessionDescription* answer = new webrtc::JsepSessionDescription(
      SessionDescriptionInterface::kAnswer);
  EXPECT_TRUE(answer->Initialize(sdp, NULL));
  cricket::ContentInfo* data_info =
      answer->description()->GetContentByName("data");
  data_info->rejected = true;

  DoSetRemoteDescription(answer);
  EXPECT_EQ(DataChannelInterface::kClosed, offer_channel->state());
}

// Test that we can create a session description from an SDP string from
// FireFox, use it as a remote session description, generate an answer and use
// the answer as a local description.
TEST_F(PeerConnectionInterfaceTest, ReceiveFireFoxOffer) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints constraints;
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
                           true);
  CreatePeerConnection(&constraints);
  AddAudioVideoStream(kStreamLabel1, "audio_label", "video_label");
  SessionDescriptionInterface* desc =
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       webrtc::kFireFoxSdpOffer);
  EXPECT_TRUE(DoSetSessionDescription(desc, false));
  CreateAnswerAsLocalDescription();
  ASSERT_TRUE(pc_->local_description() != NULL);
  ASSERT_TRUE(pc_->remote_description() != NULL);

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(pc_->local_description()->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);

  content =
      cricket::GetFirstVideoContent(pc_->local_description()->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);
#ifdef HAVE_SCTP
  content =
      cricket::GetFirstDataContent(pc_->local_description()->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_TRUE(content->rejected);
#endif
}

// Test that we can create an audio only offer and receive an answer with a
// limited set of audio codecs and receive an updated offer with more audio
// codecs, where the added codecs are not supported.
TEST_F(PeerConnectionInterfaceTest, ReceiveUpdatedAudioOfferWithBadCodecs) {
  CreatePeerConnection();
  AddVoiceStream("audio_label");
  CreateOfferAsLocalDescription();

  SessionDescriptionInterface* answer =
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kAnswer,
                                       webrtc::kAudioSdp);
  EXPECT_TRUE(DoSetSessionDescription(answer, false));

  SessionDescriptionInterface* updated_offer =
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       webrtc::kAudioSdpWithUnsupportedCodecs);
  EXPECT_TRUE(DoSetSessionDescription(updated_offer, false));
  CreateAnswerAsLocalDescription();
}

// Test that PeerConnection::Close changes the states to closed and all remote
// tracks change state to ended.
TEST_F(PeerConnectionInterfaceTest, CloseAndTestStreamsAndStates) {
  // Initialize a PeerConnection and negotiate local and remote session
  // description.
  InitiateCall();
  ASSERT_EQ(1u, pc_->local_streams()->count());
  ASSERT_EQ(1u, pc_->remote_streams()->count());

  pc_->Close();

  EXPECT_EQ(PeerConnectionInterface::kClosed, pc_->signaling_state());
  EXPECT_EQ(PeerConnectionInterface::kIceConnectionClosed,
            pc_->ice_connection_state());
  EXPECT_EQ(PeerConnectionInterface::kIceGatheringComplete,
            pc_->ice_gathering_state());

  EXPECT_EQ(1u, pc_->local_streams()->count());
  EXPECT_EQ(1u, pc_->remote_streams()->count());

  scoped_refptr<MediaStreamInterface> remote_stream =
          pc_->remote_streams()->at(0);
  EXPECT_EQ(MediaStreamTrackInterface::kEnded,
            remote_stream->GetVideoTracks()[0]->state());
  EXPECT_EQ(MediaStreamTrackInterface::kEnded,
            remote_stream->GetAudioTracks()[0]->state());
}

// Test that PeerConnection methods fails gracefully after
// PeerConnection::Close has been called.
TEST_F(PeerConnectionInterfaceTest, CloseAndTestMethods) {
  CreatePeerConnection();
  AddAudioVideoStream(kStreamLabel1, "audio_label", "video_label");
  CreateOfferAsRemoteDescription();
  CreateAnswerAsLocalDescription();

  ASSERT_EQ(1u, pc_->local_streams()->count());
  scoped_refptr<MediaStreamInterface> local_stream =
      pc_->local_streams()->at(0);

  pc_->Close();

  pc_->RemoveStream(local_stream);
  EXPECT_FALSE(pc_->AddStream(local_stream, NULL));

  ASSERT_FALSE(local_stream->GetAudioTracks().empty());
  talk_base::scoped_refptr<webrtc::DtmfSenderInterface> dtmf_sender(
      pc_->CreateDtmfSender(local_stream->GetAudioTracks()[0]));
  EXPECT_TRUE(NULL == dtmf_sender);  // local stream has been removed.

  EXPECT_TRUE(pc_->CreateDataChannel("test", NULL) == NULL);

  EXPECT_TRUE(pc_->local_description() != NULL);
  EXPECT_TRUE(pc_->remote_description() != NULL);

  talk_base::scoped_ptr<SessionDescriptionInterface> offer;
  EXPECT_TRUE(DoCreateOffer(offer.use()));
  talk_base::scoped_ptr<SessionDescriptionInterface> answer;
  EXPECT_TRUE(DoCreateAnswer(answer.use()));

  std::string sdp;
  ASSERT_TRUE(pc_->remote_description()->ToString(&sdp));
  SessionDescriptionInterface* remote_offer =
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       sdp, NULL);
  EXPECT_FALSE(DoSetRemoteDescription(remote_offer));

  ASSERT_TRUE(pc_->local_description()->ToString(&sdp));
  SessionDescriptionInterface* local_offer =
        webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                         sdp, NULL);
  EXPECT_FALSE(DoSetLocalDescription(local_offer));
}

// Test that GetStats can still be called after PeerConnection::Close.
TEST_F(PeerConnectionInterfaceTest, CloseAndGetStats) {
  InitiateCall();
  pc_->Close();
  DoGetStats(NULL);
}
