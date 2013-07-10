/*
 * libjingle
 * Copyright 2004, Google Inc.
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
#include <sstream>
#include <iostream>
#include "common/common.h"
#include "buzz/xmppengine.h"
#include "buzz/xmlelement.h"
#include "buzz/chatroommodule.h"
#include "buzz/constants.h"
#include "engine/util_unittest.h"
#include "test/unittest.h"
#include "test/unittest-inl.h"

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
    UNUSED(room);
    ss_ <<"[ChatroomEnteredStatus status: ";
    WriteEnteredStatus(ss_, status);
    ss_ <<"]";
  }


  void ChatroomExitedStatus(XmppChatroomModule* room,
                            XmppChatroomExitedStatus status) {
    UNUSED(room);
    ss_ <<"[ChatroomExitedStatus status: ";
    WriteExitedStatus(ss_, status);
    ss_ <<"]";
  }

  void MemberEntered(XmppChatroomModule* room, 
                          const XmppChatroomMember* entered_member) {
    UNUSED(room);
    ss_ << "[MemberEntered " << entered_member->member_jid().Str() << "]";
  }

  void MemberExited(XmppChatroomModule* room,
                         const XmppChatroomMember* exited_member) {
    UNUSED(room);
    ss_ << "[MemberExited " << exited_member->member_jid().Str() << "]";
  }

  void MemberChanged(XmppChatroomModule* room,
      const XmppChatroomMember* changed_member) {
    UNUSED(room);
    ss_ << "[MemberChanged " << changed_member->member_jid().Str() << "]";
  }

  virtual void MessageReceived(XmppChatroomModule* room, const XmlElement& message) {
    UNUSED2(room, message);
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
    scoped_ptr<XmppEngine> engine(XmppEngine::Create());
    XmppTestHandler handler(engine.get());

    // Configure the module and handler
    scoped_ptr<XmppChatroomModule> chatroom(XmppChatroomModule::Create());

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
