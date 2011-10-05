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

#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/webrtcsession.h"
#include "talk/app/webrtc_dev/peerconnectionsignaling.h"
#include "talk/base/thread.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/p2p/client/fakeportallocator.h"

class MockWebRtcSessionObserver : public webrtc::WebRtcSessionObserver {
 public:
  virtual void OnCandidatesReady(
      const std::vector<cricket::Candidate>& candidates) {
    for (cricket::Candidates::const_iterator iter = candidates.begin();
         iter != candidates.end(); ++iter) {
      candidates_.push_back(*iter);
    }
  }
  std::vector<cricket::Candidate> candidates_;
};

class WebRtcSessionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    signaling_thread_ = talk_base::Thread::Current();
    worker_thread_ = talk_base::Thread::Current();
    channel_manager_.reset(new cricket::ChannelManager(worker_thread_));
    port_allocator_.reset(
        new cricket::FakePortAllocator(worker_thread_, NULL));
    desc_factory_.reset(
        new cricket::MediaSessionDescriptionFactory(channel_manager_.get()));
  }

  bool InitializeSession() {
    return session_.get()->Initialize();
  }

  bool CheckChannels() {
    return (session_->voice_channel() != NULL &&
            session_->video_channel() != NULL);
  }

  bool CheckTransportChannels() {
    EXPECT_TRUE(session_->GetChannel(cricket::CN_AUDIO, "rtp") != NULL);
    EXPECT_TRUE(session_->GetChannel(cricket::CN_AUDIO, "rtcp") != NULL);
    EXPECT_TRUE(session_->GetChannel(cricket::CN_VIDEO, "video_rtp") != NULL);
    EXPECT_TRUE(session_->GetChannel(cricket::CN_VIDEO, "video_rtcp") != NULL);
  }

  void Init() {
    ASSERT_TRUE(channel_manager_.get() != NULL);
    ASSERT_TRUE(session_.get() == NULL);
    EXPECT_TRUE(channel_manager_.get()->Init());
    session_.reset(new webrtc::WebRtcSession(
        channel_manager_.get(), worker_thread_, signaling_thread_,
        port_allocator_.get()));
    session_->RegisterObserver(&observer_);
    desc_provider_ = session_.get();
    EXPECT_TRUE(InitializeSession());
  }

  // Creates an offer with one source ssrc, if ssrc = 0 no source info
  // video ssrc + 1
  void CreateOffer(uint32 ssrc) {
    cricket::MediaSessionOptions options;
    options.is_video = true;
    if (ssrc != 0) {
      options.audio_sources.push_back(cricket::SourceParam(ssrc, "", ""));
      ++ssrc;
      options.video_sources.push_back(cricket::SourceParam(ssrc, "", ""));
    }
    local_desc_ = desc_provider_->ProvideOffer(options);
    ASSERT_TRUE(local_desc_ != NULL);
  }
  void CreateAnswer(uint32 ssrc) {
    cricket::MediaSessionOptions options;
    options.is_video = true;
    if (ssrc != 0) {
      options.audio_sources.push_back(cricket::SourceParam(ssrc, "", ""));
      ++ssrc;
      options.video_sources.push_back(cricket::SourceParam(ssrc, "", ""));
    }
    remote_desc_ = desc_factory_->CreateAnswer(local_desc_, options);
    ASSERT_TRUE(remote_desc_ != NULL);
  }
  void SetRemoteContents() {
    desc_provider_->SetRemoteSessionDescription(
        remote_desc_, observer_.candidates_);
  }
  void NegotiationDone() {
    desc_provider_->NegotiationDone();
  }

  const cricket::SessionDescription* local_desc_;
  const cricket::SessionDescription* remote_desc_;
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<webrtc::WebRtcSession> session_;
  webrtc::SessionDescriptionProvider* desc_provider_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  talk_base::scoped_ptr<cricket::MediaSessionDescriptionFactory> desc_factory_;
  MockWebRtcSessionObserver observer_;
};

TEST_F(WebRtcSessionTest, TestInitialize) {
  WebRtcSessionTest::Init();
  EXPECT_TRUE(CheckChannels());
  CheckTransportChannels();
  talk_base::Thread::Current()->ProcessMessages(1000);
  EXPECT_EQ(4u, observer_.candidates_.size());
}

// TODO(mallinath) - Adding test cases for session.
TEST_F(WebRtcSessionTest, DISABLE_TestOfferAnswer) {
  WebRtcSessionTest::Init();
  EXPECT_TRUE(CheckChannels());
  CheckTransportChannels();
  talk_base::Thread::Current()->ProcessMessages(1);
}

