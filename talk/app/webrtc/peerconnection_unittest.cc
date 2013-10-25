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

#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <vector>

#include "talk/app/webrtc/dtmfsender.h"
#include "talk/app/webrtc/fakeportallocatorfactory.h"
#include "talk/app/webrtc/localaudiosource.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/test/fakeaudiocapturemodule.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/test/fakevideotrackrenderer.h"
#include "talk/app/webrtc/test/fakeperiodicvideocapturer.h"
#include "talk/app/webrtc/test/mockpeerconnectionobservers.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/thread.h"
#include "talk/media/webrtc/fakewebrtcvideoengine.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/session/media/mediasession.h"

#define MAYBE_SKIP_TEST(feature)                    \
  if (!(feature())) {                               \
    LOG(LS_INFO) << "Feature disabled... skipping"; \
    return;                                         \
  }

using cricket::ContentInfo;
using cricket::FakeWebRtcVideoDecoder;
using cricket::FakeWebRtcVideoDecoderFactory;
using cricket::FakeWebRtcVideoEncoder;
using cricket::FakeWebRtcVideoEncoderFactory;
using cricket::MediaContentDescription;
using webrtc::DataBuffer;
using webrtc::DataChannelInterface;
using webrtc::DtmfSender;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::FakeConstraints;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::MockCreateSessionDescriptionObserver;
using webrtc::MockDataChannelObserver;
using webrtc::MockSetSessionDescriptionObserver;
using webrtc::MockStatsObserver;
using webrtc::SessionDescriptionInterface;
using webrtc::StreamCollectionInterface;

static const int kMaxWaitMs = 1000;
static const int kMaxWaitForStatsMs = 3000;
static const int kMaxWaitForFramesMs = 5000;
static const int kEndAudioFrameCount = 3;
static const int kEndVideoFrameCount = 3;

static const char kStreamLabelBase[] = "stream_label";
static const char kVideoTrackLabelBase[] = "video_track";
static const char kAudioTrackLabelBase[] = "audio_track";
static const char kDataChannelLabel[] = "data_channel";

static void RemoveLinesFromSdp(const std::string& line_start,
                               std::string* sdp) {
  const char kSdpLineEnd[] = "\r\n";
  size_t ssrc_pos = 0;
  while ((ssrc_pos = sdp->find(line_start, ssrc_pos)) !=
      std::string::npos) {
    size_t end_ssrc = sdp->find(kSdpLineEnd, ssrc_pos);
    sdp->erase(ssrc_pos, end_ssrc - ssrc_pos + strlen(kSdpLineEnd));
  }
}

class SignalingMessageReceiver {
 public:
 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class JsepMessageReceiver : public SignalingMessageReceiver {
 public:
  virtual void ReceiveSdpMessage(const std::string& type,
                                 std::string& msg) = 0;
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) = 0;

 protected:
  JsepMessageReceiver() {}
  virtual ~JsepMessageReceiver() {}
};

template <typename MessageReceiver>
class PeerConnectionTestClientBase
    : public webrtc::PeerConnectionObserver,
      public MessageReceiver {
 public:
  ~PeerConnectionTestClientBase() {
    while (!fake_video_renderers_.empty()) {
      RenderMap::iterator it = fake_video_renderers_.begin();
      delete it->second;
      fake_video_renderers_.erase(it);
    }
  }

  virtual void Negotiate()  = 0;

  virtual void Negotiate(bool audio, bool video)  = 0;

  virtual void SetVideoConstraints(
      const webrtc::FakeConstraints& video_constraint) {
    video_constraints_ = video_constraint;
  }

  void AddMediaStream(bool audio, bool video) {
    std::string label = kStreamLabelBase +
        talk_base::ToString<int>(
            static_cast<int>(peer_connection_->local_streams()->count()));
    talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream(label);

    if (audio && can_receive_audio()) {
      FakeConstraints constraints;
      // Disable highpass filter so that we can get all the test audio frames.
      constraints.AddMandatory(
          MediaConstraintsInterface::kHighpassFilter, false);
      talk_base::scoped_refptr<webrtc::AudioSourceInterface> source =
          peer_connection_factory_->CreateAudioSource(&constraints);
      // TODO(perkj): Test audio source when it is implemented. Currently audio
      // always use the default input.
      talk_base::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          peer_connection_factory_->CreateAudioTrack(kAudioTrackLabelBase,
                                                     source));
      stream->AddTrack(audio_track);
    }
    if (video && can_receive_video()) {
      stream->AddTrack(CreateLocalVideoTrack(label));
    }

    EXPECT_TRUE(peer_connection_->AddStream(stream, NULL));
  }

  size_t NumberOfLocalMediaStreams() {
    return peer_connection_->local_streams()->count();
  }

  bool SessionActive() {
    return peer_connection_->signaling_state() ==
        webrtc::PeerConnectionInterface::kStable;
  }

  void set_signaling_message_receiver(
      MessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void EnableVideoDecoderFactory() {
    video_decoder_factory_enabled_ = true;
    fake_video_decoder_factory_->AddSupportedVideoCodecType(
        webrtc::kVideoCodecVP8);
  }

  bool AudioFramesReceivedCheck(int number_of_frames) const {
    return number_of_frames <= fake_audio_capture_module_->frames_received();
  }

  bool VideoFramesReceivedCheck(int number_of_frames) {
    if (video_decoder_factory_enabled_) {
      const std::vector<FakeWebRtcVideoDecoder*>& decoders
          = fake_video_decoder_factory_->decoders();
      if (decoders.empty()) {
        return number_of_frames <= 0;
      }

      for (std::vector<FakeWebRtcVideoDecoder*>::const_iterator
           it = decoders.begin(); it != decoders.end(); ++it) {
        if (number_of_frames > (*it)->GetNumFramesReceived()) {
          return false;
        }
      }
      return true;
    } else {
      if (fake_video_renderers_.empty()) {
        return number_of_frames <= 0;
      }

      for (RenderMap::const_iterator it = fake_video_renderers_.begin();
           it != fake_video_renderers_.end(); ++it) {
        if (number_of_frames > it->second->num_rendered_frames()) {
          return false;
        }
      }
      return true;
    }
  }
  // Verify the CreateDtmfSender interface
  void VerifyDtmf() {
    talk_base::scoped_ptr<DummyDtmfObserver> observer(new DummyDtmfObserver());
    talk_base::scoped_refptr<DtmfSenderInterface> dtmf_sender;

    // We can't create a DTMF sender with an invalid audio track or a non local
    // track.
    EXPECT_TRUE(peer_connection_->CreateDtmfSender(NULL) == NULL);
    talk_base::scoped_refptr<webrtc::AudioTrackInterface> non_localtrack(
        peer_connection_factory_->CreateAudioTrack("dummy_track",
                                                   NULL));
    EXPECT_TRUE(peer_connection_->CreateDtmfSender(non_localtrack) == NULL);

    // We should be able to create a DTMF sender from a local track.
    webrtc::AudioTrackInterface* localtrack =
        peer_connection_->local_streams()->at(0)->GetAudioTracks()[0];
    dtmf_sender = peer_connection_->CreateDtmfSender(localtrack);
    EXPECT_TRUE(dtmf_sender.get() != NULL);
    dtmf_sender->RegisterObserver(observer.get());

    // Test the DtmfSender object just created.
    EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
    EXPECT_TRUE(dtmf_sender->InsertDtmf("1a", 100, 50));

    // We don't need to verify that the DTMF tones are actually sent out because
    // that is already covered by the tests of the lower level components.

    EXPECT_TRUE_WAIT(observer->completed(), kMaxWaitMs);
    std::vector<std::string> tones;
    tones.push_back("1");
    tones.push_back("a");
    tones.push_back("");
    observer->Verify(tones);

    dtmf_sender->UnregisterObserver();
  }

  // Verifies that the SessionDescription have rejected the appropriate media
  // content.
  void VerifyRejectedMediaInSessionDescription() {
    ASSERT_TRUE(peer_connection_->remote_description() != NULL);
    ASSERT_TRUE(peer_connection_->local_description() != NULL);
    const cricket::SessionDescription* remote_desc =
        peer_connection_->remote_description()->description();
    const cricket::SessionDescription* local_desc =
        peer_connection_->local_description()->description();

    const ContentInfo* remote_audio_content = GetFirstAudioContent(remote_desc);
    if (remote_audio_content) {
      const ContentInfo* audio_content =
          GetFirstAudioContent(local_desc);
      EXPECT_EQ(can_receive_audio(), !audio_content->rejected);
    }

    const ContentInfo* remote_video_content = GetFirstVideoContent(remote_desc);
    if (remote_video_content) {
      const ContentInfo* video_content =
          GetFirstVideoContent(local_desc);
      EXPECT_EQ(can_receive_video(), !video_content->rejected);
    }
  }

  void SetExpectIceRestart(bool expect_restart) {
    expect_ice_restart_ = expect_restart;
  }

  bool ExpectIceRestart() const { return expect_ice_restart_; }

  void VerifyLocalIceUfragAndPassword() {
    ASSERT_TRUE(peer_connection_->local_description() != NULL);
    const cricket::SessionDescription* desc =
        peer_connection_->local_description()->description();
    const cricket::ContentInfos& contents = desc->contents();

    for (size_t index = 0; index < contents.size(); ++index) {
      if (contents[index].rejected)
        continue;
      const cricket::TransportDescription* transport_desc =
          desc->GetTransportDescriptionByName(contents[index].name);

      std::map<int, IceUfragPwdPair>::const_iterator ufragpair_it =
          ice_ufrag_pwd_.find(static_cast<int>(index));
      if (ufragpair_it == ice_ufrag_pwd_.end()) {
        ASSERT_FALSE(ExpectIceRestart());
        ice_ufrag_pwd_[static_cast<int>(index)] =
            IceUfragPwdPair(transport_desc->ice_ufrag, transport_desc->ice_pwd);
      } else if (ExpectIceRestart()) {
        const IceUfragPwdPair& ufrag_pwd = ufragpair_it->second;
        EXPECT_NE(ufrag_pwd.first, transport_desc->ice_ufrag);
        EXPECT_NE(ufrag_pwd.second, transport_desc->ice_pwd);
      } else {
        const IceUfragPwdPair& ufrag_pwd = ufragpair_it->second;
        EXPECT_EQ(ufrag_pwd.first, transport_desc->ice_ufrag);
        EXPECT_EQ(ufrag_pwd.second, transport_desc->ice_pwd);
      }
    }
  }

  int GetAudioOutputLevelStats(webrtc::MediaStreamTrackInterface* track) {
    talk_base::scoped_refptr<MockStatsObserver>
        observer(new talk_base::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(observer, track));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->AudioOutputLevel();
  }

  int GetAudioInputLevelStats() {
    talk_base::scoped_refptr<MockStatsObserver>
        observer(new talk_base::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(observer, NULL));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->AudioInputLevel();
  }

  int GetBytesReceivedStats(webrtc::MediaStreamTrackInterface* track) {
    talk_base::scoped_refptr<MockStatsObserver>
    observer(new talk_base::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(observer, track));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->BytesReceived();
  }

  int GetBytesSentStats(webrtc::MediaStreamTrackInterface* track) {
    talk_base::scoped_refptr<MockStatsObserver>
    observer(new talk_base::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(observer, track));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->BytesSent();
  }

  int rendered_width() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty() ? 1 :
        fake_video_renderers_.begin()->second->width();
  }

  int rendered_height() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty() ? 1 :
        fake_video_renderers_.begin()->second->height();
  }

  size_t number_of_remote_streams() {
    if (!pc())
      return 0;
    return pc()->remote_streams()->count();
  }

  StreamCollectionInterface* remote_streams() {
    if (!pc()) {
      ADD_FAILURE();
      return NULL;
    }
    return pc()->remote_streams();
  }

  StreamCollectionInterface* local_streams() {
    if (!pc()) {
      ADD_FAILURE();
      return NULL;
    }
    return pc()->local_streams();
  }

  webrtc::PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

  // PeerConnectionObserver callbacks.
  virtual void OnError() {}
  virtual void OnMessage(const std::string&) {}
  virtual void OnSignalingMessage(const std::string& /*msg*/) {}
  virtual void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) {
    EXPECT_EQ(peer_connection_->signaling_state(), new_state);
  }
  virtual void OnAddStream(webrtc::MediaStreamInterface* media_stream) {
    for (size_t i = 0; i < media_stream->GetVideoTracks().size(); ++i) {
      const std::string id = media_stream->GetVideoTracks()[i]->id();
      ASSERT_TRUE(fake_video_renderers_.find(id) ==
          fake_video_renderers_.end());
      fake_video_renderers_[id] = new webrtc::FakeVideoTrackRenderer(
          media_stream->GetVideoTracks()[i]);
    }
  }
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* media_stream) {}
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) {
    EXPECT_EQ(peer_connection_->ice_connection_state(), new_state);
  }
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    EXPECT_EQ(peer_connection_->ice_gathering_state(), new_state);
  }
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* /*candidate*/) {}

  webrtc::PeerConnectionInterface* pc() {
    return peer_connection_.get();
  }

 protected:
  explicit PeerConnectionTestClientBase(const std::string& id)
      : id_(id),
        expect_ice_restart_(false),
        fake_video_decoder_factory_(NULL),
        fake_video_encoder_factory_(NULL),
        video_decoder_factory_enabled_(false),
        signaling_message_receiver_(NULL) {
  }
  bool Init(const MediaConstraintsInterface* constraints) {
    EXPECT_TRUE(!peer_connection_);
    EXPECT_TRUE(!peer_connection_factory_);
    allocator_factory_ = webrtc::FakePortAllocatorFactory::Create();
    if (!allocator_factory_) {
      return false;
    }
    audio_thread_.Start();
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create(
        &audio_thread_);

    if (fake_audio_capture_module_ == NULL) {
      return false;
    }
    fake_video_decoder_factory_ = new FakeWebRtcVideoDecoderFactory();
    fake_video_encoder_factory_ = new FakeWebRtcVideoEncoderFactory();
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        talk_base::Thread::Current(), talk_base::Thread::Current(),
        fake_audio_capture_module_, fake_video_encoder_factory_,
        fake_video_decoder_factory_);
    if (!peer_connection_factory_) {
      return false;
    }
    peer_connection_ = CreatePeerConnection(allocator_factory_.get(),
                                            constraints);
    return peer_connection_.get() != NULL;
  }
  virtual talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(webrtc::PortAllocatorFactoryInterface* factory,
                           const MediaConstraintsInterface* constraints) = 0;
  MessageReceiver* signaling_message_receiver() {
    return signaling_message_receiver_;
  }
  webrtc::PeerConnectionFactoryInterface* peer_connection_factory() {
    return peer_connection_factory_.get();
  }

  virtual bool can_receive_audio() = 0;
  virtual bool can_receive_video() = 0;
  const std::string& id() const { return id_; }

 private:
  class DummyDtmfObserver : public DtmfSenderObserverInterface {
   public:
    DummyDtmfObserver() : completed_(false) {}

    // Implements DtmfSenderObserverInterface.
    void OnToneChange(const std::string& tone) {
      tones_.push_back(tone);
      if (tone.empty()) {
        completed_ = true;
      }
    }

    void Verify(const std::vector<std::string>& tones) const {
      ASSERT_TRUE(tones_.size() == tones.size());
      EXPECT_TRUE(std::equal(tones.begin(), tones.end(), tones_.begin()));
    }

    bool completed() const { return completed_; }

   private:
    bool completed_;
    std::vector<std::string> tones_;
  };

  talk_base::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrack(const std::string stream_label) {
    // Set max frame rate to 10fps to reduce the risk of the tests to be flaky.
    FakeConstraints source_constraints = video_constraints_;
    source_constraints.SetMandatoryMaxFrameRate(10);

    talk_base::scoped_refptr<webrtc::VideoSourceInterface> source =
        peer_connection_factory_->CreateVideoSource(
            new webrtc::FakePeriodicVideoCapturer(),
            &source_constraints);
    std::string label = stream_label + kVideoTrackLabelBase;
    return peer_connection_factory_->CreateVideoTrack(label, source);
  }

  std::string id_;
  // Separate thread for executing |fake_audio_capture_module_| tasks. Audio
  // processing must not be performed on the same thread as signaling due to
  // signaling time constraints and relative complexity of the audio pipeline.
  // This is consistent with the video pipeline that us a a separate thread for
  // encoding and decoding.
  talk_base::Thread audio_thread_;

  talk_base::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
      allocator_factory_;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;

  typedef std::pair<std::string, std::string> IceUfragPwdPair;
  std::map<int, IceUfragPwdPair> ice_ufrag_pwd_;
  bool expect_ice_restart_;

  // Needed to keep track of number of frames send.
  talk_base::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;
  // Needed to keep track of number of frames received.
  typedef std::map<std::string, webrtc::FakeVideoTrackRenderer*> RenderMap;
  RenderMap fake_video_renderers_;
  // Needed to keep track of number of frames received when external decoder
  // used.
  FakeWebRtcVideoDecoderFactory* fake_video_decoder_factory_;
  FakeWebRtcVideoEncoderFactory* fake_video_encoder_factory_;
  bool video_decoder_factory_enabled_;
  webrtc::FakeConstraints video_constraints_;

  // For remote peer communication.
  MessageReceiver* signaling_message_receiver_;
};

class JsepTestClient
    : public PeerConnectionTestClientBase<JsepMessageReceiver> {
 public:
  static JsepTestClient* CreateClient(
      const std::string& id,
      const MediaConstraintsInterface* constraints) {
    JsepTestClient* client(new JsepTestClient(id));
    if (!client->Init(constraints)) {
      delete client;
      return NULL;
    }
    return client;
  }
  ~JsepTestClient() {}

  virtual void Negotiate() {
    Negotiate(true, true);
  }
  virtual void Negotiate(bool audio, bool video) {
    talk_base::scoped_ptr<SessionDescriptionInterface> offer;
    EXPECT_TRUE(DoCreateOffer(offer.use()));

    if (offer->description()->GetContentByName("audio")) {
      offer->description()->GetContentByName("audio")->rejected = !audio;
    }
    if (offer->description()->GetContentByName("video")) {
      offer->description()->GetContentByName("video")->rejected = !video;
    }

    std::string sdp;
    EXPECT_TRUE(offer->ToString(&sdp));
    EXPECT_TRUE(DoSetLocalDescription(offer.release()));
    signaling_message_receiver()->ReceiveSdpMessage(
        webrtc::SessionDescriptionInterface::kOffer, sdp);
  }
  // JsepMessageReceiver callback.
  virtual void ReceiveSdpMessage(const std::string& type,
                                 std::string& msg) {
    FilterIncomingSdpMessage(&msg);
    if (type == webrtc::SessionDescriptionInterface::kOffer) {
      HandleIncomingOffer(msg);
    } else {
      HandleIncomingAnswer(msg);
    }
  }
  // JsepMessageReceiver callback.
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) {
    LOG(INFO) << id() << "ReceiveIceMessage";
    talk_base::scoped_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, msg, NULL));
    EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
  }
  // Implements PeerConnectionObserver functions needed by Jsep.
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    LOG(INFO) << id() << "OnIceCandidate";

    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver() == NULL) {
      // Remote party may be deleted.
      return;
    }
    signaling_message_receiver()->ReceiveIceMessage(candidate->sdp_mid(),
        candidate->sdp_mline_index(), ice_sdp);
  }

  void IceRestart() {
    session_description_constraints_.SetMandatoryIceRestart(true);
    SetExpectIceRestart(true);
  }

  void SetReceiveAudioVideo(bool audio, bool video) {
    SetReceiveAudio(audio);
    SetReceiveVideo(video);
    ASSERT_EQ(audio, can_receive_audio());
    ASSERT_EQ(video, can_receive_video());
  }

  void SetReceiveAudio(bool audio) {
    if (audio && can_receive_audio())
      return;
    session_description_constraints_.SetMandatoryReceiveAudio(audio);
  }

  void SetReceiveVideo(bool video) {
    if (video && can_receive_video())
      return;
    session_description_constraints_.SetMandatoryReceiveVideo(video);
  }

  void RemoveMsidFromReceivedSdp(bool remove) {
    remove_msid_ = remove;
  }

  void RemoveSdesCryptoFromReceivedSdp(bool remove) {
    remove_sdes_ = remove;
  }

  void RemoveBundleFromReceivedSdp(bool remove) {
    remove_bundle_ = remove;
  }

  virtual bool can_receive_audio() {
    bool value;
    if (webrtc::FindConstraint(&session_description_constraints_,
        MediaConstraintsInterface::kOfferToReceiveAudio, &value, NULL)) {
      return value;
    }
    return true;
  }

  virtual bool can_receive_video() {
    bool value;
    if (webrtc::FindConstraint(&session_description_constraints_,
        MediaConstraintsInterface::kOfferToReceiveVideo, &value, NULL)) {
      return value;
    }
    return true;
  }

  virtual void OnIceComplete() {
    LOG(INFO) << id() << "OnIceComplete";
  }

  virtual void OnDataChannel(DataChannelInterface* data_channel) {
    LOG(INFO) << id() << "OnDataChannel";
    data_channel_ = data_channel;
    data_observer_.reset(new MockDataChannelObserver(data_channel));
  }

  void CreateDataChannel() {
    data_channel_ = pc()->CreateDataChannel(kDataChannelLabel,
                                                         NULL);
    ASSERT_TRUE(data_channel_.get() != NULL);
    data_observer_.reset(new MockDataChannelObserver(data_channel_));
  }

  DataChannelInterface* data_channel() { return data_channel_; }
  const MockDataChannelObserver* data_observer() const {
    return data_observer_.get();
  }

 protected:
  explicit JsepTestClient(const std::string& id)
      : PeerConnectionTestClientBase<JsepMessageReceiver>(id),
        remove_msid_(false),
        remove_bundle_(false),
        remove_sdes_(false) {
  }

  virtual talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(webrtc::PortAllocatorFactoryInterface* factory,
                           const MediaConstraintsInterface* constraints) {
    // CreatePeerConnection with IceServers.
    webrtc::PeerConnectionInterface::IceServers ice_servers;
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    ice_servers.push_back(ice_server);
    return peer_connection_factory()->CreatePeerConnection(
        ice_servers, constraints, factory, NULL, this);
  }

  void HandleIncomingOffer(const std::string& msg) {
    LOG(INFO) << id() << "HandleIncomingOffer ";
    if (NumberOfLocalMediaStreams() == 0) {
      // If we are not sending any streams ourselves it is time to add some.
      AddMediaStream(true, true);
    }
    talk_base::scoped_ptr<SessionDescriptionInterface> desc(
         webrtc::CreateSessionDescription("offer", msg, NULL));
    EXPECT_TRUE(DoSetRemoteDescription(desc.release()));
    talk_base::scoped_ptr<SessionDescriptionInterface> answer;
    EXPECT_TRUE(DoCreateAnswer(answer.use()));
    std::string sdp;
    EXPECT_TRUE(answer->ToString(&sdp));
    EXPECT_TRUE(DoSetLocalDescription(answer.release()));
    if (signaling_message_receiver()) {
      signaling_message_receiver()->ReceiveSdpMessage(
          webrtc::SessionDescriptionInterface::kAnswer, sdp);
    }
  }

  void HandleIncomingAnswer(const std::string& msg) {
    LOG(INFO) << id() << "HandleIncomingAnswer";
    talk_base::scoped_ptr<SessionDescriptionInterface> desc(
         webrtc::CreateSessionDescription("answer", msg, NULL));
    EXPECT_TRUE(DoSetRemoteDescription(desc.release()));
  }

  bool DoCreateOfferAnswer(SessionDescriptionInterface** desc,
                           bool offer) {
    talk_base::scoped_refptr<MockCreateSessionDescriptionObserver>
        observer(new talk_base::RefCountedObject<
            MockCreateSessionDescriptionObserver>());
    if (offer) {
      pc()->CreateOffer(observer, &session_description_constraints_);
    } else {
      pc()->CreateAnswer(observer, &session_description_constraints_);
    }
    EXPECT_EQ_WAIT(true, observer->called(), kMaxWaitMs);
    *desc = observer->release_desc();
    if (observer->result() && ExpectIceRestart()) {
      EXPECT_EQ(0u, (*desc)->candidates(0)->count());
    }
    return observer->result();
  }

  bool DoCreateOffer(SessionDescriptionInterface** desc) {
    return DoCreateOfferAnswer(desc, true);
  }

  bool DoCreateAnswer(SessionDescriptionInterface** desc) {
    return DoCreateOfferAnswer(desc, false);
  }

  bool DoSetLocalDescription(SessionDescriptionInterface* desc) {
    talk_base::scoped_refptr<MockSetSessionDescriptionObserver>
            observer(new talk_base::RefCountedObject<
                MockSetSessionDescriptionObserver>());
    LOG(INFO) << id() << "SetLocalDescription ";
    pc()->SetLocalDescription(observer, desc);
    // Ignore the observer result. If we wait for the result with
    // EXPECT_TRUE_WAIT, local ice candidates might be sent to the remote peer
    // before the offer which is an error.
    // The reason is that EXPECT_TRUE_WAIT uses
    // talk_base::Thread::Current()->ProcessMessages(1);
    // ProcessMessages waits at least 1ms but processes all messages before
    // returning. Since this test is synchronous and send messages to the remote
    // peer whenever a callback is invoked, this can lead to messages being
    // sent to the remote peer in the wrong order.
    // TODO(perkj): Find a way to check the result without risking that the
    // order of sent messages are changed. Ex- by posting all messages that are
    // sent to the remote peer.
    return true;
  }

  bool DoSetRemoteDescription(SessionDescriptionInterface* desc) {
    talk_base::scoped_refptr<MockSetSessionDescriptionObserver>
        observer(new talk_base::RefCountedObject<
            MockSetSessionDescriptionObserver>());
    LOG(INFO) << id() << "SetRemoteDescription ";
    pc()->SetRemoteDescription(observer, desc);
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->result();
  }

  // This modifies all received SDP messages before they are processed.
  void FilterIncomingSdpMessage(std::string* sdp) {
    if (remove_msid_) {
      const char kSdpSsrcAttribute[] = "a=ssrc:";
      RemoveLinesFromSdp(kSdpSsrcAttribute, sdp);
      const char kSdpMsidSupportedAttribute[] = "a=msid-semantic:";
      RemoveLinesFromSdp(kSdpMsidSupportedAttribute, sdp);
    }
    if (remove_bundle_) {
      const char kSdpBundleAttribute[] = "a=group:BUNDLE";
      RemoveLinesFromSdp(kSdpBundleAttribute, sdp);
    }
    if (remove_sdes_) {
      const char kSdpSdesCryptoAttribute[] = "a=crypto";
      RemoveLinesFromSdp(kSdpSdesCryptoAttribute, sdp);
    }
  }

 private:
  webrtc::FakeConstraints session_description_constraints_;
  bool remove_msid_;  // True if MSID should be removed in received SDP.
  bool remove_bundle_;  // True if bundle should be removed in received SDP.
  bool remove_sdes_;  // True if a=crypto should be removed in received SDP.

  talk_base::scoped_refptr<DataChannelInterface> data_channel_;
  talk_base::scoped_ptr<MockDataChannelObserver> data_observer_;
};

template <typename SignalingClass>
class P2PTestConductor : public testing::Test {
 public:
  bool SessionActive() {
    return initiating_client_->SessionActive() &&
        receiving_client_->SessionActive();
  }
  // Return true if the number of frames provided have been received or it is
  // known that that will never occur (e.g. no frames will be sent or
  // captured).
  bool FramesNotPending(int audio_frames_to_receive,
                        int video_frames_to_receive) {
    return VideoFramesReceivedCheck(video_frames_to_receive) &&
        AudioFramesReceivedCheck(audio_frames_to_receive);
  }
  bool AudioFramesReceivedCheck(int frames_received) {
    return initiating_client_->AudioFramesReceivedCheck(frames_received) &&
        receiving_client_->AudioFramesReceivedCheck(frames_received);
  }
  bool VideoFramesReceivedCheck(int frames_received) {
    return initiating_client_->VideoFramesReceivedCheck(frames_received) &&
        receiving_client_->VideoFramesReceivedCheck(frames_received);
  }
  void VerifyDtmf() {
    initiating_client_->VerifyDtmf();
    receiving_client_->VerifyDtmf();
  }

  void TestUpdateOfferWithRejectedContent() {
    initiating_client_->Negotiate(true, false);
    EXPECT_TRUE_WAIT(
        FramesNotPending(kEndAudioFrameCount * 2, kEndVideoFrameCount),
        kMaxWaitForFramesMs);
    // There shouldn't be any more video frame after the new offer is
    // negotiated.
    EXPECT_FALSE(VideoFramesReceivedCheck(kEndVideoFrameCount + 1));
  }

  void VerifyRenderedSize(int width, int height) {
    EXPECT_EQ(width, receiving_client()->rendered_width());
    EXPECT_EQ(height, receiving_client()->rendered_height());
    EXPECT_EQ(width, initializing_client()->rendered_width());
    EXPECT_EQ(height, initializing_client()->rendered_height());
  }

  void VerifySessionDescriptions() {
    initiating_client_->VerifyRejectedMediaInSessionDescription();
    receiving_client_->VerifyRejectedMediaInSessionDescription();
    initiating_client_->VerifyLocalIceUfragAndPassword();
    receiving_client_->VerifyLocalIceUfragAndPassword();
  }

  P2PTestConductor() {
    talk_base::InitializeSSL(NULL);
  }
  ~P2PTestConductor() {
    if (initiating_client_) {
      initiating_client_->set_signaling_message_receiver(NULL);
    }
    if (receiving_client_) {
      receiving_client_->set_signaling_message_receiver(NULL);
    }
    talk_base::CleanupSSL();
  }

  bool CreateTestClients() {
    return CreateTestClients(NULL, NULL);
  }

  bool CreateTestClients(MediaConstraintsInterface* init_constraints,
                         MediaConstraintsInterface* recv_constraints) {
    initiating_client_.reset(SignalingClass::CreateClient("Caller: ",
                                                          init_constraints));
    receiving_client_.reset(SignalingClass::CreateClient("Callee: ",
                                                         recv_constraints));
    if (!initiating_client_ || !receiving_client_) {
      return false;
    }
    initiating_client_->set_signaling_message_receiver(receiving_client_.get());
    receiving_client_->set_signaling_message_receiver(initiating_client_.get());
    return true;
  }

  void SetVideoConstraints(const webrtc::FakeConstraints& init_constraints,
                           const webrtc::FakeConstraints& recv_constraints) {
    initiating_client_->SetVideoConstraints(init_constraints);
    receiving_client_->SetVideoConstraints(recv_constraints);
  }

  void EnableVideoDecoderFactory() {
    initiating_client_->EnableVideoDecoderFactory();
    receiving_client_->EnableVideoDecoderFactory();
  }

  // This test sets up a call between two parties. Both parties send static
  // frames to each other. Once the test is finished the number of sent frames
  // is compared to the number of received frames.
  void LocalP2PTest() {
    if (initiating_client_->NumberOfLocalMediaStreams() == 0) {
      initiating_client_->AddMediaStream(true, true);
    }
    initiating_client_->Negotiate();
    const int kMaxWaitForActivationMs = 5000;
    // Assert true is used here since next tests are guaranteed to fail and
    // would eat up 5 seconds.
    ASSERT_TRUE_WAIT(SessionActive(), kMaxWaitForActivationMs);
    VerifySessionDescriptions();


    int audio_frame_count = kEndAudioFrameCount;
    // TODO(ronghuawu): Add test to cover the case of sendonly and recvonly.
    if (!initiating_client_->can_receive_audio() ||
        !receiving_client_->can_receive_audio()) {
      audio_frame_count = -1;
    }
    int video_frame_count = kEndVideoFrameCount;
    if (!initiating_client_->can_receive_video() ||
        !receiving_client_->can_receive_video()) {
      video_frame_count = -1;
    }

    if (audio_frame_count != -1 || video_frame_count != -1) {
      // Audio or video is expected to flow, so both sides should get to the
      // Connected state.
      // Note: These tests have been observed to fail under heavy load at
      // shorter timeouts, so they may be flaky.
      EXPECT_EQ_WAIT(
          webrtc::PeerConnectionInterface::kIceConnectionConnected,
          initiating_client_->ice_connection_state(),
          kMaxWaitForFramesMs);
      EXPECT_EQ_WAIT(
          webrtc::PeerConnectionInterface::kIceConnectionConnected,
          receiving_client_->ice_connection_state(),
          kMaxWaitForFramesMs);
    }

    if (initiating_client_->can_receive_audio() ||
        initiating_client_->can_receive_video()) {
      // The initiating client can receive media, so it must produce candidates
      // that will serve as destinations for that media.
      // TODO(bemasc): Understand why the state is not already Complete here, as
      // seems to be the case for the receiving client. This may indicate a bug
      // in the ICE gathering system.
      EXPECT_NE(webrtc::PeerConnectionInterface::kIceGatheringNew,
                initiating_client_->ice_gathering_state());
    }
    if (receiving_client_->can_receive_audio() ||
        receiving_client_->can_receive_video()) {
      EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                     receiving_client_->ice_gathering_state(),
                     kMaxWaitForFramesMs);
    }

    EXPECT_TRUE_WAIT(FramesNotPending(audio_frame_count, video_frame_count),
                     kMaxWaitForFramesMs);
  }

  SignalingClass* initializing_client() { return initiating_client_.get(); }
  SignalingClass* receiving_client() { return receiving_client_.get(); }

 private:
  talk_base::scoped_ptr<SignalingClass> initiating_client_;
  talk_base::scoped_ptr<SignalingClass> receiving_client_;
};
typedef P2PTestConductor<JsepTestClient> JsepPeerConnectionP2PTestClient;

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
#if !defined(THREAD_SANITIZER)

// This test sets up a Jsep call between two parties and test Dtmf.
// TODO(holmer): Disabled due to sometimes crashing on buildbots.
// See issue webrtc/2378.
TEST_F(JsepPeerConnectionP2PTestClient, DISABLED_LocalP2PTestDtmf) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  VerifyDtmf();
}

// This test sets up a Jsep call between two parties and test that we can get a
// video aspect ratio of 16:9.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTest16To9) {
  ASSERT_TRUE(CreateTestClients());
  FakeConstraints constraint;
  double requested_ratio = 640.0/360;
  constraint.SetMandatoryMinAspectRatio(requested_ratio);
  SetVideoConstraints(constraint, constraint);
  LocalP2PTest();

  ASSERT_LE(0, initializing_client()->rendered_height());
  double initiating_video_ratio =
      static_cast<double>(initializing_client()->rendered_width()) /
      initializing_client()->rendered_height();
  EXPECT_LE(requested_ratio, initiating_video_ratio);

  ASSERT_LE(0, receiving_client()->rendered_height());
  double receiving_video_ratio =
      static_cast<double>(receiving_client()->rendered_width()) /
      receiving_client()->rendered_height();
  EXPECT_LE(requested_ratio, receiving_video_ratio);
}

// This test sets up a Jsep call between two parties and test that the
// received video has a resolution of 1280*720.
// TODO(mallinath): Enable when
// http://code.google.com/p/webrtc/issues/detail?id=981 is fixed.
TEST_F(JsepPeerConnectionP2PTestClient, DISABLED_LocalP2PTest1280By720) {
  ASSERT_TRUE(CreateTestClients());
  FakeConstraints constraint;
  constraint.SetMandatoryMinWidth(1280);
  constraint.SetMandatoryMinHeight(720);
  SetVideoConstraints(constraint, constraint);
  LocalP2PTest();
  VerifyRenderedSize(1280, 720);
}

// This test sets up a call between two endpoints that are configured to use
// DTLS key agreement. As a result, DTLS is negotiated and used for transport.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestDtls) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  LocalP2PTest();
  VerifyRenderedSize(640, 480);
}

// This test sets up a audio call initially and then upgrades to audio/video,
// using DTLS.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestDtlsRenegotiate) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  receiving_client()->SetReceiveAudioVideo(true, false);
  LocalP2PTest();
  receiving_client()->SetReceiveAudioVideo(true, true);
  receiving_client()->Negotiate();
}

// This test sets up a call between an endpoint configured to use either SDES or
// DTLS (the offerer) and just SDES (the answerer). As a result, SDES is used
// instead of DTLS.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestOfferDtlsToSdes) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, NULL));
  LocalP2PTest();
  VerifyRenderedSize(640, 480);
}

// This test sets up a call between an endpoint configured to use SDES
// (the offerer) and either SDES or DTLS (the answerer). As a result, SDES is
// used instead of DTLS.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestOfferSdesToDtls) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(NULL, &setup_constraints));
  LocalP2PTest();
  VerifyRenderedSize(640, 480);
}

// This test sets up a call between two endpoints that are configured to use
// DTLS key agreement. The offerer don't support SDES. As a result, DTLS is
// negotiated and used for transport.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestOfferDtlsButNotSdes) {
  MAYBE_SKIP_TEST(talk_base::SSLStreamAdapter::HaveDtlsSrtp);
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  receiving_client()->RemoveSdesCryptoFromReceivedSdp(true);
  LocalP2PTest();
  VerifyRenderedSize(640, 480);
}

// This test sets up a Jsep call between two parties, and the callee only
// accept to receive video.
// BUG=https://code.google.com/p/webrtc/issues/detail?id=2288
TEST_F(JsepPeerConnectionP2PTestClient, DISABLED_LocalP2PTestAnswerVideo) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(false, true);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties, and the callee only
// accept to receive audio.
TEST_F(JsepPeerConnectionP2PTestClient, DISABLED_LocalP2PTestAnswerAudio) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(true, false);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties, and the callee reject both
// audio and video.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestAnswerNone) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(false, false);
  LocalP2PTest();
}

// This test sets up an audio and video call between two parties. After the call
// runs for a while (10 frames), the caller sends an update offer with video
// being rejected. Once the re-negotiation is done, the video flow should stop
// and the audio flow should continue.
TEST_F(JsepPeerConnectionP2PTestClient, UpdateOfferWithRejectedContent) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  TestUpdateOfferWithRejectedContent();
}

// This test sets up a Jsep call between two parties. The MSID is removed from
// the SDP strings from the caller.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestWithoutMsid) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->RemoveMsidFromReceivedSdp(true);
  // TODO(perkj): Currently there is a bug that cause audio to stop playing if
  // audio and video is muxed when MSID is disabled. Remove
  // SetRemoveBundleFromSdp once
  // https://code.google.com/p/webrtc/issues/detail?id=1193 is fixed.
  receiving_client()->RemoveBundleFromReceivedSdp(true);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties and the initiating peer
// sends two steams.
// TODO(perkj): Disabled due to
// https://code.google.com/p/webrtc/issues/detail?id=1454
TEST_F(JsepPeerConnectionP2PTestClient, DISABLED_LocalP2PTestTwoStreams) {
  ASSERT_TRUE(CreateTestClients());
  // Set optional video constraint to max 320pixels to decrease CPU usage.
  FakeConstraints constraint;
  constraint.SetOptionalMaxWidth(320);
  SetVideoConstraints(constraint, constraint);
  initializing_client()->AddMediaStream(true, true);
  initializing_client()->AddMediaStream(false, true);
  ASSERT_EQ(2u, initializing_client()->NumberOfLocalMediaStreams());
  LocalP2PTest();
  EXPECT_EQ(2u, receiving_client()->number_of_remote_streams());
}

// Test that we can receive the audio output level from a remote audio track.
TEST_F(JsepPeerConnectionP2PTestClient, GetAudioOutputLevelStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* remote_streams =
      initializing_client()->remote_streams();
  ASSERT_GT(remote_streams->count(), 0u);
  ASSERT_GT(remote_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];

  // Get the audio output level stats. Note that the level is not available
  // until a RTCP packet has been received.
  EXPECT_TRUE_WAIT(
      initializing_client()->GetAudioOutputLevelStats(remote_audio_track) > 0,
      kMaxWaitForStatsMs);
}

// Test that an audio input level is reported.
TEST_F(JsepPeerConnectionP2PTestClient, GetAudioInputLevelStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  // Get the audio input level stats.  The level should be available very
  // soon after the test starts.
  EXPECT_TRUE_WAIT(initializing_client()->GetAudioInputLevelStats() > 0,
      kMaxWaitForStatsMs);
}

// Test that we can get incoming byte counts from both audio and video tracks.
TEST_F(JsepPeerConnectionP2PTestClient, GetBytesReceivedStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* remote_streams =
      initializing_client()->remote_streams();
  ASSERT_GT(remote_streams->count(), 0u);
  ASSERT_GT(remote_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesReceivedStats(remote_audio_track) > 0,
      kMaxWaitForStatsMs);

  MediaStreamTrackInterface* remote_video_track =
      remote_streams->at(0)->GetVideoTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesReceivedStats(remote_video_track) > 0,
      kMaxWaitForStatsMs);
}

// Test that we can get outgoing byte counts from both audio and video tracks.
TEST_F(JsepPeerConnectionP2PTestClient, GetBytesSentStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* local_streams =
      initializing_client()->local_streams();
  ASSERT_GT(local_streams->count(), 0u);
  ASSERT_GT(local_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* local_audio_track =
      local_streams->at(0)->GetAudioTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesSentStats(local_audio_track) > 0,
      kMaxWaitForStatsMs);

  MediaStreamTrackInterface* local_video_track =
      local_streams->at(0)->GetVideoTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesSentStats(local_video_track) > 0,
      kMaxWaitForStatsMs);
}

// This test sets up a call between two parties with audio, video and data.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestDataChannel) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  initializing_client()->CreateDataChannel();
  LocalP2PTest();
  ASSERT_TRUE(initializing_client()->data_channel() != NULL);
  ASSERT_TRUE(receiving_client()->data_channel() != NULL);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);

  std::string data = "hello world";
  initializing_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, receiving_client()->data_observer()->last_message(),
                 kMaxWaitMs);
  receiving_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, initializing_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  receiving_client()->data_channel()->Close();
  // Send new offer and answer.
  receiving_client()->Negotiate();
  EXPECT_FALSE(initializing_client()->data_observer()->IsOpen());
  EXPECT_FALSE(receiving_client()->data_observer()->IsOpen());
}

// This test sets up a call between two parties and creates a data channel.
// The test tests that received data is buffered unless an observer has been
// registered.
// Rtp data channels can receive data before the underlying
// transport has detected that a channel is writable and thus data can be
// received before the data channel state changes to open. That is hard to test
// but the same buffering is used in that case.
TEST_F(JsepPeerConnectionP2PTestClient, RegisterDataChannelObserver) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  initializing_client()->CreateDataChannel();
  initializing_client()->Negotiate();

  ASSERT_TRUE(initializing_client()->data_channel() != NULL);
  ASSERT_TRUE(receiving_client()->data_channel() != NULL);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_EQ_WAIT(DataChannelInterface::kOpen,
                 receiving_client()->data_channel()->state(), kMaxWaitMs);

  // Unregister the existing observer.
  receiving_client()->data_channel()->UnregisterObserver();
  std::string data = "hello world";
  initializing_client()->data_channel()->Send(DataBuffer(data));
  // Wait a while to allow the sent data to arrive before an observer is
  // registered..
  talk_base::Thread::Current()->ProcessMessages(100);

  MockDataChannelObserver new_observer(receiving_client()->data_channel());
  EXPECT_EQ_WAIT(data, new_observer.last_message(), kMaxWaitMs);
}

// This test sets up a call between two parties with audio, video and but only
// the initiating client support data.
TEST_F(JsepPeerConnectionP2PTestClient, LocalP2PTestReceiverDoesntSupportData) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, NULL));
  initializing_client()->CreateDataChannel();
  LocalP2PTest();
  EXPECT_TRUE(initializing_client()->data_channel() != NULL);
  EXPECT_FALSE(receiving_client()->data_channel());
  EXPECT_FALSE(initializing_client()->data_observer()->IsOpen());
}

// This test sets up a call between two parties with audio, video. When audio
// and video is setup and flowing and data channel is negotiated.
TEST_F(JsepPeerConnectionP2PTestClient, AddDataChannelAfterRenegotiation) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  LocalP2PTest();
  initializing_client()->CreateDataChannel();
  // Send new offer and answer.
  initializing_client()->Negotiate();
  ASSERT_TRUE(initializing_client()->data_channel() != NULL);
  ASSERT_TRUE(receiving_client()->data_channel() != NULL);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
}

// This test sets up a call between two parties with audio, and video.
// During the call, the initializing side restart ice and the test verifies that
// new ice candidates are generated and audio and video still can flow.
TEST_F(JsepPeerConnectionP2PTestClient, IceRestart) {
  ASSERT_TRUE(CreateTestClients());

  // Negotiate and wait for ice completion and make sure audio and video plays.
  LocalP2PTest();

  // Create a SDP string of the first audio candidate for both clients.
  const webrtc::IceCandidateCollection* audio_candidates_initiator =
      initializing_client()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_receiver =
      receiving_client()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_initiator->count(), 0u);
  ASSERT_GT(audio_candidates_receiver->count(), 0u);
  std::string initiator_candidate;
  EXPECT_TRUE(
      audio_candidates_initiator->at(0)->ToString(&initiator_candidate));
  std::string receiver_candidate;
  EXPECT_TRUE(audio_candidates_receiver->at(0)->ToString(&receiver_candidate));

  // Restart ice on the initializing client.
  receiving_client()->SetExpectIceRestart(true);
  initializing_client()->IceRestart();

  // Negotiate and wait for ice completion again and make sure audio and video
  // plays.
  LocalP2PTest();

  // Create a SDP string of the first audio candidate for both clients again.
  const webrtc::IceCandidateCollection* audio_candidates_initiator_restart =
      initializing_client()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_reciever_restart =
      receiving_client()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_initiator_restart->count(), 0u);
  ASSERT_GT(audio_candidates_reciever_restart->count(), 0u);
  std::string initiator_candidate_restart;
  EXPECT_TRUE(audio_candidates_initiator_restart->at(0)->ToString(
      &initiator_candidate_restart));
  std::string receiver_candidate_restart;
  EXPECT_TRUE(audio_candidates_reciever_restart->at(0)->ToString(
      &receiver_candidate_restart));

  // Verify that the first candidates in the local session descriptions has
  // changed.
  EXPECT_NE(initiator_candidate, initiator_candidate_restart);
  EXPECT_NE(receiver_candidate, receiver_candidate_restart);
}


// This test sets up a Jsep call between two parties with external
// VideoDecoderFactory.
// TODO(holmer): Disabled due to sometimes crashing on buildbots.
// See issue webrtc/2378.
TEST_F(JsepPeerConnectionP2PTestClient,
       DISABLED_LocalP2PTestWithVideoDecoderFactory) {
  ASSERT_TRUE(CreateTestClients());
  EnableVideoDecoderFactory();
  LocalP2PTest();
}

#endif // if !defined(THREAD_SANITIZER)

