/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <sstream>
#include <string>
#include "buzz/chatroommodule.h"
#include "buzz/constants.h"
#include "buzz/xmlelement.h"
#include "buzz/xmppengine.h"
#include "common/common.h"
#include "engine/util_unittest.h"
#include "test/unittest-inl.h"
#include "test/unittest.h"

#define TEST_OK(x) TEST_EQ((x),XMPP_RETURN_OK)
#define TEST_BADARGUMENT(x) TEST_EQ((x),XMPP_RETURN_BADARGUMENT)

namespace buzz {

class MultiUserChatModuleTest;

static void
WriteEnteredStatus(std::ostream& os, XmppChatroomEnteredStatus status) {
  switch(status) {
    case XMPP_CHATROOM_ENTERED_SUCCESS:
      os<<"success";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_NICKNAME_CONFLICT:
      os<<"failure(nickname conflict)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_REQUIRED:
      os<<"failure(password required)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_INCORRECT:
      os<<"failure(password incorrect)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_NOT_A_MEMBER:
      os<<"failure(not a member)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BANNED:
      os<<"failure(member banned)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_MAX_USERS:
      os<<"failure(max users)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_ROOM_LOCKED:
      os<<"failure(room locked)";
      break;
    case XMPP_CHATROOM_ENTERED_FAILURE_UNSPECIFIED:
      os<<"failure(unspecified)";
      break;
    default:
      os<<"unknown";
      break;
  } 
}

static void
WriteExitedStatus(std::ostream& os, XmppChatroomExitedStatus status) {
  switch (status) {
    case XMPP_CHATROOM_EXITED_REQUESTED:
      os<<"requested";
      break;
    case XMPP_CHATROOM_EXITED_BANNED:
      os<<"banned";
      break;
    case XMPP_CHATROOM_EXITED_KICKED:
      os<<"kicked";
      break;
    case XMPP_CHATROOM_EXITED_NOT_A_MEMBER:
      os<<"not member";
      break;
    case XMPP_CHATROOM_EXITED_SYSTEM_SHUTDOWN:
      os<<"system shutdown";
      break;
    case XMPP_CHATROOM_EXITED_UNSPECIFIED:
      os<<"unspecified";
      break;
    default:
      os<<"unknown";
      break;
  }
}

//! This session handler saves all calls to a string.  These are events and
//! data delivered form the engine to application code.
class XmppTestChatroomHandler : public XmppChatroomHandler {
public:
  XmppTestChatroomHandler() {}
  virtual ~XmppTestChatroomHandler() {}

  void ChatroomEnteredStatus(XmppChatroomModule* room,
                             XmppChatroomEnteredStatus status) {
    RTC_UNUSED(room);
    ss_ <<"[ChatroomEnteredStatus status: ";
    WriteEnteredStatus(ss_, status);
    ss_ <<"]";
  }


  void ChatroomExitedStatus(XmppChatroomModule* room,
                            XmppChatroomExitedStatus status) {
    RTC_UNUSED(room);
    ss_ <<"[ChatroomExitedStatus status: ";
    WriteExitedStatus(ss_, status);
    ss_ <<"]";
  }

  void MemberEntered(XmppChatroomModule* room, 
                          const XmppChatroomMember* entered_member) {
    RTC_UNUSED(room);
    ss_ << "[MemberEntered " << entered_member->member_jid().Str() << "]";
  }

  void MemberExited(XmppChatroomModule* room,
                         const XmppChatroomMember* exited_member) {
    RTC_UNUSED(room);
    ss_ << "[MemberExited " << exited_member->member_jid().Str() << "]";
  }

  void MemberChanged(XmppChatroomModule* room,
      const XmppChatroomMember* changed_member) {
    RTC_UNUSED(room);
    ss_ << "[MemberChanged " << changed_member->member_jid().Str() << "]";
  }

  virtual void MessageReceived(XmppChatroomModule* room, const XmlElement& message) {
    RTC_UNUSED2(room, message);
  }

 
  std::string Str() {
    return ss_.str();
  }

  std::string StrClear() {
    std::string result = ss_.str();
    ss_.str("");
    return result;
  }

private:
  std::stringstream ss_;
};

//! This is the class that holds all of the unit test code for the
//! roster module
class XmppChatroomModuleTest : public UnitTest {
public:
  XmppChatroomModuleTest() {}

  void TestEnterExitChatroom() {
    std::stringstream dump;

    // Configure the engine
    rtc::scoped_ptr<XmppEngine> engine(XmppEngine::Create());
    XmppTestHandler handler(engine.get());

    // Configure the module and handler
    rtc::scoped_ptr<XmppChatroomModule> chatroom(XmppChatroomModule::Create());

    // Configure the module handler
    chatroom->RegisterEngine(engine.get());

    // Set up callbacks
    engine->SetOutputHandler(&handler);
    engine->AddStanzaHandler(&handler);
    engine->SetSessionHandler(&handler);

    // Set up minimal login info
    engine->SetUser(Jid("david@my-server"));
    engine->SetPassword("david");

    // Do the whole login handshake
    RunLogin(this, engine.get(), &handler);
    TEST_EQ("", handler.OutputActivity());

    // Get the chatroom and set the handler
    XmppTestChatroomHandler chatroom_handler;
    chatroom->set_chatroom_handler(static_cast<XmppChatroomHandler*>(&chatroom_handler));

    // try to enter the chatroom
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_NOT_IN_ROOM);
    chatroom->set_nickname("thirdwitch");
    chatroom->set_chatroom_jid(Jid("darkcave@my-server"));
    chatroom->RequestEnterChatroom("", XMPP_CONNECTION_STATUS_UNKNOWN, "en");
    TEST_EQ(chatroom_handler.StrClear(), "");
    TEST_EQ(handler.OutputActivity(),
      "<presence to=\"darkcave@my-server/thirdwitch\">"
        "<muc:x xmlns:muc=\"http://jabber.org/protocol/muc\"/>"
      "</presence>");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_REQUESTED_ENTER);

    // simulate the server and test the client
    std::string input;
    input = "<presence from=\"darkcave@my-server/firstwitch\" to=\"david@my-server\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"owner\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(), "");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_REQUESTED_ENTER);

    input = "<presence from=\"darkcave@my-server/secondwitch\" to=\"david@my-server\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"member\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(), "");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_REQUESTED_ENTER);

    input = "<presence from=\"darkcave@my-server/thirdwitch\" to=\"david@my-server\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"member\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(),
      "[ChatroomEnteredStatus status: success]");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_IN_ROOM);

    // simulate somebody else entering the room after we entered
    input = "<presence from=\"darkcave@my-server/fourthwitch\" to=\"david@my-server\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"member\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(), "[MemberEntered darkcave@my-server/fourthwitch]");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_IN_ROOM);

    // simulate somebody else leaving the room after we entered
    input = "<presence from=\"darkcave@my-server/secondwitch\" to=\"david@my-server\" type=\"unavailable\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"member\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(), "[MemberExited darkcave@my-server/secondwitch]");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_IN_ROOM);

    // try to leave the room
    chatroom->RequestExitChatroom();
    TEST_EQ(chatroom_handler.StrClear(), "");
    TEST_EQ(handler.OutputActivity(),
      "<presence to=\"darkcave@my-server/thirdwitch\" type=\"unavailable\"/>");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_REQUESTED_EXIT);

    // simulate the server and test the client
    input = "<presence from=\"darkcave@my-server/thirdwitch\" to=\"david@my-server\" type=\"unavailable\">"
             "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
              "<item affiliation=\"member\" role=\"participant\"/>"
             "</x>"
            "</presence>";
    TEST_OK(engine->HandleInput(input.c_str(), input.length()));
    TEST_EQ(chatroom_handler.StrClear(),
      "[ChatroomExitedStatus status: requested]");
    TEST_EQ(chatroom->state(), XMPP_CHATROOM_STATE_NOT_IN_ROOM);
  }

};

// A global function that creates the test suite for this set of tests.
TestBase* ChatroomModuleTest_Create() {
  TestSuite* suite = new TestSuite("ChatroomModuleTest");
  ADD_TEST(suite, XmppChatroomModuleTest, TestEnterExitChatroom);
  return suite;
}

}
