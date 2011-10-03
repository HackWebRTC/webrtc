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

class MockPeerConnectionSignaling {

};

class WebRtcSessionTest : public testing::Test {
 public:
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
    pc_signaling_.reset(
        new webrtc::PeerConnectionSignaling(channel_manager_.get(),
                                            signaling_thread_));
  }

  bool InitializeSession() {
    return session_.get()->Initialize();
  }
  bool CheckChannels() {
    return (session_->voice_channel() != NULL &&
            session_->video_channel() != NULL);
  }

  void Init() {
    ASSERT_TRUE(channel_manager_.get() != NULL);
    ASSERT_TRUE(session_.get() == NULL);
    EXPECT_TRUE(channel_manager_.get()->Init());
    session_.reset(new webrtc::WebRtcSession(
        channel_manager_.get(), worker_thread_, signaling_thread_,
        port_allocator_.get(), pc_signaling_.get()));
    EXPECT_TRUE(InitializeSession());
    EXPECT_TRUE(CheckChannels());
  }

 private:
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  talk_base::scoped_ptr<cricket::PortAllocator> port_allocator_;
  talk_base::scoped_ptr<webrtc::PeerConnectionSignaling> pc_signaling_;
  talk_base::scoped_ptr<webrtc::WebRtcSession> session_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

TEST_F(WebRtcSessionTest, TestInitialize) {
  WebRtcSessionTest::Init();
}
