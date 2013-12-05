/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/app/webrtc/fakeportallocatorfactory.h"
#include "talk/app/webrtc/test/fakeperiodicvideocapturer.h"
#include "talk/app/webrtc/test/mockpeerconnectionobservers.h"
#include "talk/app/webrtc/test/peerconnectiontestwrapper.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/base/gunit.h"

static const char kStreamLabelBase[] = "stream_label";
static const char kVideoTrackLabelBase[] = "video_track";
static const char kAudioTrackLabelBase[] = "audio_track";
static const int kMaxWait = 5000;
static const int kTestAudioFrameCount = 3;
static const int kTestVideoFrameCount = 3;

using webrtc::FakeConstraints;
using webrtc::FakeVideoTrackRenderer;
using webrtc::IceCandidateInterface;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::MockSetSessionDescriptionObserver;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoTrackInterface;

void PeerConnectionTestWrapper::Connect(PeerConnectionTestWrapper* caller,
                                        PeerConnectionTestWrapper* callee) {
  caller->SignalOnIceCandidateReady.connect(
      callee, &PeerConnectionTestWrapper::AddIceCandidate);
  callee->SignalOnIceCandidateReady.connect(
      caller, &PeerConnectionTestWrapper::AddIceCandidate);

  caller->SignalOnSdpReady.connect(
      callee, &PeerConnectionTestWrapper::ReceiveOfferSdp);
  callee->SignalOnSdpReady.connect(
      caller, &PeerConnectionTestWrapper::ReceiveAnswerSdp);
}

PeerConnectionTestWrapper::PeerConnectionTestWrapper(const std::string& name)
    : name_(name) {}

PeerConnectionTestWrapper::~PeerConnectionTestWrapper() {}

bool PeerConnectionTestWrapper::CreatePc(
  const MediaConstraintsInterface* constraints) {
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

  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      talk_base::Thread::Current(), talk_base::Thread::Current(),
      fake_audio_capture_module_, NULL, NULL);
  if (!peer_connection_factory_) {
    return false;
  }

  // CreatePeerConnection with IceServers.
  webrtc::PeerConnectionInterface::IceServers ice_servers;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = "stun:stun.l.google.com:19302";
  ice_servers.push_back(ice_server);
  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      ice_servers, constraints, allocator_factory_.get(), NULL, this);

  return peer_connection_.get() != NULL;
}

void PeerConnectionTestWrapper::OnAddStream(MediaStreamInterface* stream) {
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": OnAddStream";
  // TODO(ronghuawu): support multiple streams.
  if (stream->GetVideoTracks().size() > 0) {
    renderer_.reset(new FakeVideoTrackRenderer(stream->GetVideoTracks()[0]));
  }
}

void PeerConnectionTestWrapper::OnIceCandidate(
    const IceCandidateInterface* candidate) {
  std::string sdp;
  EXPECT_TRUE(candidate->ToString(&sdp));
  // Give the user a chance to modify sdp for testing.
  SignalOnIceCandidateCreated(&sdp);
  SignalOnIceCandidateReady(candidate->sdp_mid(), candidate->sdp_mline_index(),
                            sdp);
}

void PeerConnectionTestWrapper::OnSuccess(SessionDescriptionInterface* desc) {
  // This callback should take the ownership of |desc|.
  talk_base::scoped_ptr<SessionDescriptionInterface> owned_desc(desc);
  std::string sdp;
  EXPECT_TRUE(desc->ToString(&sdp));

  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": " << desc->type() << " sdp created: " << sdp;

  // Give the user a chance to modify sdp for testing.
  SignalOnSdpCreated(&sdp);

  SetLocalDescription(desc->type(), sdp);

  SignalOnSdpReady(sdp);
}

void PeerConnectionTestWrapper::CreateOffer(
    const MediaConstraintsInterface* constraints) {
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": CreateOffer.";
  peer_connection_->CreateOffer(this, constraints);
}

void PeerConnectionTestWrapper::CreateAnswer(
    const MediaConstraintsInterface* constraints) {
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": CreateAnswer.";
  peer_connection_->CreateAnswer(this, constraints);
}

void PeerConnectionTestWrapper::ReceiveOfferSdp(const std::string& sdp) {
  SetRemoteDescription(SessionDescriptionInterface::kOffer, sdp);
  CreateAnswer(NULL);
}

void PeerConnectionTestWrapper::ReceiveAnswerSdp(const std::string& sdp) {
  SetRemoteDescription(SessionDescriptionInterface::kAnswer, sdp);
}

void PeerConnectionTestWrapper::SetLocalDescription(const std::string& type,
                                                    const std::string& sdp) {
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": SetLocalDescription " << type << " " << sdp;

  talk_base::scoped_refptr<MockSetSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
                   MockSetSessionDescriptionObserver>());
  peer_connection_->SetLocalDescription(
      observer, webrtc::CreateSessionDescription(type, sdp, NULL));
}

void PeerConnectionTestWrapper::SetRemoteDescription(const std::string& type,
                                                     const std::string& sdp) {
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": SetRemoteDescription " << type << " " << sdp;

  talk_base::scoped_refptr<MockSetSessionDescriptionObserver>
      observer(new talk_base::RefCountedObject<
                   MockSetSessionDescriptionObserver>());
  peer_connection_->SetRemoteDescription(
      observer, webrtc::CreateSessionDescription(type, sdp, NULL));
}

void PeerConnectionTestWrapper::AddIceCandidate(const std::string& sdp_mid,
                                                int sdp_mline_index,
                                                const std::string& candidate) {
  talk_base::scoped_ptr<webrtc::IceCandidateInterface> owned_candidate(
      webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, NULL));
  EXPECT_TRUE(peer_connection_->AddIceCandidate(owned_candidate.get()));
}

void PeerConnectionTestWrapper::WaitForCallEstablished() {
  WaitForConnection();
  WaitForAudio();
  WaitForVideo();
}

void PeerConnectionTestWrapper::WaitForConnection() {
  EXPECT_TRUE_WAIT(CheckForConnection(), kMaxWait);
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": Connected.";
}

bool PeerConnectionTestWrapper::CheckForConnection() {
  return (peer_connection_->ice_connection_state() ==
          PeerConnectionInterface::kIceConnectionConnected);
}

void PeerConnectionTestWrapper::WaitForAudio() {
  EXPECT_TRUE_WAIT(CheckForAudio(), kMaxWait);
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": Got enough audio frames.";
}

bool PeerConnectionTestWrapper::CheckForAudio() {
  return (fake_audio_capture_module_->frames_received() >=
          kTestAudioFrameCount);
}

void PeerConnectionTestWrapper::WaitForVideo() {
  EXPECT_TRUE_WAIT(CheckForVideo(), kMaxWait);
  LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
               << ": Got enough video frames.";
}

bool PeerConnectionTestWrapper::CheckForVideo() {
  if (!renderer_) {
    return false;
  }
  return (renderer_->num_rendered_frames() >= kTestVideoFrameCount);
}

void PeerConnectionTestWrapper::GetAndAddUserMedia(
    bool audio, const webrtc::FakeConstraints& audio_constraints,
    bool video, const webrtc::FakeConstraints& video_constraints) {
  talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream =
      GetUserMedia(audio, audio_constraints, video, video_constraints);
  EXPECT_TRUE(peer_connection_->AddStream(stream, NULL));
}

talk_base::scoped_refptr<webrtc::MediaStreamInterface>
    PeerConnectionTestWrapper::GetUserMedia(
        bool audio, const webrtc::FakeConstraints& audio_constraints,
        bool video, const webrtc::FakeConstraints& video_constraints) {
  std::string label = kStreamLabelBase +
      talk_base::ToString<int>(
          static_cast<int>(peer_connection_->local_streams()->count()));
  talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream =
      peer_connection_factory_->CreateLocalMediaStream(label);

  if (audio) {
    FakeConstraints constraints = audio_constraints;
    // Disable highpass filter so that we can get all the test audio frames.
    constraints.AddMandatory(
        MediaConstraintsInterface::kHighpassFilter, false);
    talk_base::scoped_refptr<webrtc::AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(&constraints);
    talk_base::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        peer_connection_factory_->CreateAudioTrack(kAudioTrackLabelBase,
                                                   source));
    stream->AddTrack(audio_track);
  }

  if (video) {
    // Set max frame rate to 10fps to reduce the risk of the tests to be flaky.
    FakeConstraints constraints = video_constraints;
    constraints.SetMandatoryMaxFrameRate(10);

    talk_base::scoped_refptr<webrtc::VideoSourceInterface> source =
        peer_connection_factory_->CreateVideoSource(
            new webrtc::FakePeriodicVideoCapturer(), &constraints);
    std::string videotrack_label = label + kVideoTrackLabelBase;
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(videotrack_label, source));

    stream->AddTrack(video_track);
  }
  return stream;
}
