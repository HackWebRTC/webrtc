/*
 * webrtcsessionimpl_unittest.cc
 *
 *  Created on: Mar 11, 2011
 *      Author: mallinath
 */

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/app/webrtcsessionimpl.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/fakemediaengine.h"
#include "talk/session/phone/fakesession.h"

namespace webrtc {
using talk_base::scoped_ptr;

static const char* kTestSessionId = "1234";

class WebRTCSessionImplForTest : public WebRTCSessionImpl {
 public:
  WebRTCSessionImplForTest(const std::string& jid, const std::string& id,
                           const std::string& type,
                           const std::string& direction,
                           cricket::PortAllocator* allocator,
                           cricket::ChannelManager* channelmgr)
      : WebRTCSessionImpl(NULL, id, type, direction, allocator, channelmgr) {

  }

  ~WebRTCSessionImplForTest() {
    //Do Nothing
  }

  virtual cricket::Transport* GetTransport() {
    return static_cast<cricket::FakeTransport*>(WebRTCSessionImpl::GetTransport());
  }

 protected:
  virtual cricket::Transport* CreateTransport() {
    return new cricket::FakeTransport(talk_base::Thread::Current(), talk_base::Thread::Current());
  }

};

class WebRTCSessionImplTest : public sigslot::has_slots<>,
                              public testing::Test {
 public:
  WebRTCSessionImplTest() {
    network_mgr_.reset(new talk_base::NetworkManager());
    port_allocator_.reset(new cricket::BasicPortAllocator(network_mgr_.get()));
    media_engine_ = new cricket::FakeMediaEngine();
    channel_mgr_.reset(new cricket::ChannelManager(talk_base::Thread::Current()));
    channel_mgr_.reset(NULL);

  }
  ~WebRTCSessionImplTest() {

  }

  void CreateSession(const std::string& jid, const std::string& id,
                     const std::string& type, const std::string& dir) {
    session_.reset(new WebRTCSessionImplForTest(jid, id, type, dir,
                                                port_allocator_.get(),
                                                channel_mgr_.get()));
  }
  bool InitiateCall(const std::string& jid, const std::string& id,
                     const std::string& type, const std::string& dir) {
    CreateSession(jid, id, type, dir);
    bool ret = session_->Initiate();
    return ret;
  }

  bool GetCandidates() {
      return InitiateCall("", kTestSessionId, "t", "s");

  }


 protected:
  scoped_ptr<talk_base::NetworkManager> network_mgr_;
  scoped_ptr<cricket::BasicPortAllocator> port_allocator_;
  cricket::FakeMediaEngine* media_engine_;
  scoped_ptr<cricket::ChannelManager> channel_mgr_;
  scoped_ptr<WebRTCSessionImplForTest> session_;

};

TEST_F(WebRTCSessionImplTest, TestGetCandidatesCall) {
  EXPECT_TRUE(GetCandidates());
  EXPECT_EQ(cricket::Session::STATE_INIT, session_->state());
  EXPECT_EQ(kTestSessionId, session_->id());
  EXPECT_EQ(WebRTCSession::kTestType, session_->type());
  EXPECT_FALSE(session_->incoming());
}

} /* namespace webrtc */
