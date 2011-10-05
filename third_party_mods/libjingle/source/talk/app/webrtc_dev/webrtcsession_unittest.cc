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

class WebRtcSessionTest : public testing::Test,
                          public sigslot::has_slots<> {
 public:
  cricket::MediaSessionDescriptionFactory* media_factory_;
  WebRtcSessionTest() {
  }

  ~WebRtcSessionTest() {
  }

  virtual void SetUp() {
    signaling_thread_ = talk_base::Thread::Current();
    worker_thread_ = talk_base::Thread::Current();
    channel_manager_.reset(new cricket::ChannelManager(worker_thread_));
    port_allocator_.reset(
        new cricket::FakePortAllocator(worker_thread_, NULL));
    media_factory_ =
        new cricket::MediaSessionDescriptionFactory(channel_manager_.get());
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
    session_->SignalCandidatesReady.connect(
        this, &WebRtcSessionTest::OnCandidatesReady);
    EXPECT_TRUE(InitializeSession());
  }
  void OnCandidatesReady(webrtc::WebRtcSession* session,
                         cricket::Candidates& candidates) {
    for (cricket::Candidates::iterator iter = candidates.begin();
         iter != candidates.end(); ++iter) {
      local_candidates_.push_back(*iter);
    }
  }
  cricket::Candidates& local_candidates() {
    return local_candidates_;
  }
  cricket::SessionDescription* CreateOffer(bool video) {
    cricket::MediaSessionOptions options;
    options.is_video = true;
    // Source params not set
    cricket::SessionDescription* sdp = media_factory_->CreateOffer(options);
    return sdp;
  }
  cricket::SessionDescription* CreateAnswer(
      cricket::SessionDescription* offer, bool video) {
    cricket::MediaSessionOptions options;
    options.is_video = video;
    cricket::SessionDescription* sdp =
        media_factory_->CreateAnswer(offer, options);
  }

 private:
  cricket::Candidates local_candidates_;
  cricket::Candidates remote_candidates_;
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<webrtc::WebRtcSession> session_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

TEST_F(WebRtcSessionTest, TestInitialize) {
  WebRtcSessionTest::Init();
  EXPECT_TRUE(CheckChannels());
  CheckTransportChannels();
  talk_base::Thread::Current()->ProcessMessages(1000);
  EXPECT_EQ(4u, local_candidates().size());
}

