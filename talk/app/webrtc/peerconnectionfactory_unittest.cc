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
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectionfactory.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/app/webrtc/test/fakevideotrackrenderer.h"
#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcvoe.h"

using webrtc::FakeVideoTrackRenderer;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::PortAllocatorFactoryInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;

namespace {

typedef std::vector<PortAllocatorFactoryInterface::StunConfiguration>
    StunConfigurations;
typedef std::vector<PortAllocatorFactoryInterface::TurnConfiguration>
    TurnConfigurations;

static const char kStunIceServer[] = "stun:stun.l.google.com:19302";
static const char kTurnIceServer[] = "turn:test%40hello.com@test.com:1234";
static const char kTurnIceServerWithTransport[] =
    "turn:test@hello.com?transport=tcp";
static const char kSecureTurnIceServer[] =
    "turns:test@hello.com?transport=tcp";
static const char kSecureTurnIceServerWithoutTransportParam[] =
    "turns:test_no_transport@hello.com";
static const char kTurnIceServerWithNoUsernameInUri[] =
    "turn:test.com:1234";
static const char kTurnPassword[] = "turnpassword";
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTurnUsername[] = "test";

class NullPeerConnectionObserver : public PeerConnectionObserver {
 public:
  virtual void OnError() {}
  virtual void OnMessage(const std::string& msg) {}
  virtual void OnSignalingMessage(const std::string& msg) {}
  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) {}
  virtual void OnAddStream(MediaStreamInterface* stream) {}
  virtual void OnRemoveStream(MediaStreamInterface* stream) {}
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {}
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {}
};

}  // namespace

class PeerConnectionFactoryTest : public testing::Test {
  void SetUp() {
    factory_ = webrtc::CreatePeerConnectionFactory(talk_base::Thread::Current(),
                                                   talk_base::Thread::Current(),
                                                   NULL,
                                                   NULL,
                                                   NULL);

    ASSERT_TRUE(factory_.get() != NULL);
    allocator_factory_ =  webrtc::FakePortAllocatorFactory::Create();
  }

 protected:
  void VerifyStunConfigurations(StunConfigurations stun_config) {
    webrtc::FakePortAllocatorFactory* allocator =
        static_cast<webrtc::FakePortAllocatorFactory*>(
            allocator_factory_.get());
    ASSERT_TRUE(allocator != NULL);
    EXPECT_EQ(stun_config.size(), allocator->stun_configs().size());
    for (size_t i = 0; i < stun_config.size(); ++i) {
      EXPECT_EQ(stun_config[i].server.ToString(),
                allocator->stun_configs()[i].server.ToString());
    }
  }

  void VerifyTurnConfigurations(TurnConfigurations turn_config) {
    webrtc::FakePortAllocatorFactory* allocator =
        static_cast<webrtc::FakePortAllocatorFactory*>(
            allocator_factory_.get());
    ASSERT_TRUE(allocator != NULL);
    EXPECT_EQ(turn_config.size(), allocator->turn_configs().size());
    for (size_t i = 0; i < turn_config.size(); ++i) {
      EXPECT_EQ(turn_config[i].server.ToString(),
                allocator->turn_configs()[i].server.ToString());
      EXPECT_EQ(turn_config[i].username, allocator->turn_configs()[i].username);
      EXPECT_EQ(turn_config[i].password, allocator->turn_configs()[i].password);
      EXPECT_EQ(turn_config[i].transport_type,
                allocator->turn_configs()[i].transport_type);
    }
  }

  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory_;
  NullPeerConnectionObserver observer_;
  talk_base::scoped_refptr<PortAllocatorFactoryInterface> allocator_factory_;
};

// Verify creation of PeerConnection using internal ADM, video factory and
// internal libjingle threads.
TEST(PeerConnectionFactoryTestInternal, CreatePCUsingInternalModules) {
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory());

  NullPeerConnectionObserver observer;
  webrtc::PeerConnectionInterface::IceServers servers;

  talk_base::scoped_refptr<PeerConnectionInterface> pc(
      factory->CreatePeerConnection(servers, NULL, NULL, &observer));

  EXPECT_TRUE(pc.get() != NULL);
}

// This test verifies creation of PeerConnection with valid STUN and TURN
// configuration. Also verifies the URL's parsed correctly as expected.
TEST_F(PeerConnectionFactoryTest, CreatePCUsingIceServers) {
  webrtc::PeerConnectionInterface::IceServers ice_servers;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kStunIceServer;
  ice_servers.push_back(ice_server);
  ice_server.uri = kTurnIceServer;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  ice_server.uri = kTurnIceServerWithTransport;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  talk_base::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(ice_servers, NULL,
                                     allocator_factory_.get(),
                                     NULL,
                                     &observer_));
  EXPECT_TRUE(pc.get() != NULL);
  StunConfigurations stun_configs;
  webrtc::PortAllocatorFactoryInterface::StunConfiguration stun1(
      "stun.l.google.com", 19302);
  stun_configs.push_back(stun1);
  webrtc::PortAllocatorFactoryInterface::StunConfiguration stun2(
        "test.com", 1234);
  stun_configs.push_back(stun2);
  webrtc::PortAllocatorFactoryInterface::StunConfiguration stun3(
        "hello.com", kDefaultStunPort);
  stun_configs.push_back(stun3);
  VerifyStunConfigurations(stun_configs);
  TurnConfigurations turn_configs;
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn1(
      "test.com", 1234, "test@hello.com", kTurnPassword, "udp", false);
  turn_configs.push_back(turn1);
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn2(
      "hello.com", kDefaultStunPort, "test", kTurnPassword, "tcp", false);
  turn_configs.push_back(turn2);
  VerifyTurnConfigurations(turn_configs);
}

TEST_F(PeerConnectionFactoryTest, CreatePCUsingNoUsernameInUri) {
  webrtc::PeerConnectionInterface::IceServers ice_servers;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kStunIceServer;
  ice_servers.push_back(ice_server);
  ice_server.uri = kTurnIceServerWithNoUsernameInUri;
  ice_server.username = kTurnUsername;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  talk_base::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(ice_servers, NULL,
                                     allocator_factory_.get(),
                                     NULL,
                                     &observer_));
  EXPECT_TRUE(pc.get() != NULL);
  TurnConfigurations turn_configs;
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn(
      "test.com", 1234, kTurnUsername, kTurnPassword, "udp", false);
  turn_configs.push_back(turn);
  VerifyTurnConfigurations(turn_configs);
}

// This test verifies the PeerConnection created properly with TURN url which
// has transport parameter in it.
TEST_F(PeerConnectionFactoryTest, CreatePCUsingTurnUrlWithTransportParam) {
  webrtc::PeerConnectionInterface::IceServers ice_servers;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kTurnIceServerWithTransport;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  talk_base::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(ice_servers, NULL,
                                     allocator_factory_.get(),
                                     NULL,
                                     &observer_));
  EXPECT_TRUE(pc.get() != NULL);
  TurnConfigurations turn_configs;
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn(
      "hello.com", kDefaultStunPort, "test", kTurnPassword, "tcp", false);
  turn_configs.push_back(turn);
  VerifyTurnConfigurations(turn_configs);
  StunConfigurations stun_configs;
  webrtc::PortAllocatorFactoryInterface::StunConfiguration stun(
        "hello.com", kDefaultStunPort);
  stun_configs.push_back(stun);
  VerifyStunConfigurations(stun_configs);
}

TEST_F(PeerConnectionFactoryTest, CreatePCUsingSecureTurnUrl) {
  webrtc::PeerConnectionInterface::IceServers ice_servers;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kSecureTurnIceServer;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  ice_server.uri = kSecureTurnIceServerWithoutTransportParam;
  ice_server.password = kTurnPassword;
  ice_servers.push_back(ice_server);
  talk_base::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(ice_servers, NULL,
                                     allocator_factory_.get(),
                                     NULL,
                                     &observer_));
  EXPECT_TRUE(pc.get() != NULL);
  TurnConfigurations turn_configs;
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn1(
      "hello.com", kDefaultStunTlsPort, "test", kTurnPassword, "tcp", true);
  turn_configs.push_back(turn1);
  // TURNS with transport param should be default to tcp.
  webrtc::PortAllocatorFactoryInterface::TurnConfiguration turn2(
      "hello.com", kDefaultStunTlsPort, "test_no_transport",
      kTurnPassword, "tcp", true);
  turn_configs.push_back(turn2);
  VerifyTurnConfigurations(turn_configs);
}

// This test verifies the captured stream is rendered locally using a
// local video track.
TEST_F(PeerConnectionFactoryTest, LocalRendering) {
  cricket::FakeVideoCapturer* capturer = new cricket::FakeVideoCapturer();
  // The source take ownership of |capturer|.
  talk_base::scoped_refptr<VideoSourceInterface> source(
      factory_->CreateVideoSource(capturer, NULL));
  ASSERT_TRUE(source.get() != NULL);
  talk_base::scoped_refptr<VideoTrackInterface> track(
      factory_->CreateVideoTrack("testlabel", source));
  ASSERT_TRUE(track.get() != NULL);
  FakeVideoTrackRenderer local_renderer(track);

  EXPECT_EQ(0, local_renderer.num_rendered_frames());
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(1, local_renderer.num_rendered_frames());

  track->set_enabled(false);
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(1, local_renderer.num_rendered_frames());

  track->set_enabled(true);
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(2, local_renderer.num_rendered_frames());
}
