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

#include <string>
#include <vector>

#include "talk/base/faketaskrunner.h"
#include "talk/base/gunit.h"
#include "talk/base/sigslot.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/fakexmppclient.h"
#include "talk/xmpp/mucroomconfigtask.h"

class MucRoomConfigListener : public sigslot::has_slots<> {
 public:
  MucRoomConfigListener() : result_count(0), error_count(0) {}

  void OnResult(buzz::MucRoomConfigTask*) {
    ++result_count;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  int result_count;
  int error_count;
};

class MucRoomConfigTaskTest : public testing::Test {
 public:
  MucRoomConfigTaskTest() :
      room_jid("muc-jid-ponies@domain.com"),
      room_name("ponies") {
  }

  virtual void SetUp() {
    runner = new talk_base::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomConfigListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  talk_base::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomConfigListener* listener;
  buzz::Jid room_jid;
  std::string room_name;
};

TEST_F(MucRoomConfigTaskTest, TestConfigEnterprise) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  std::vector<std::string> room_features;
  room_features.push_back("feature1");
  room_features.push_back("feature2");
  buzz::MucRoomConfigTask* task = new buzz::MucRoomConfigTask(
      xmpp_client, room_jid, "ponies", room_features);
  EXPECT_EQ(room_jid, task->room_jid());

  task->SignalResult.connect(listener, &MucRoomConfigListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"muc-jid-ponies@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"http://jabber.org/protocol/muc#owner\">"
          "<x xmlns=\"jabber:x:data\" type=\"form\">"
            "<field var=\"muc#roomconfig_roomname\" type=\"text-single\">"
              "<value>ponies</value>"
            "</field>"
            "<field var=\"muc#roomconfig_features\" type=\"list-multi\">"
              "<value>feature1</value>"
              "<value>feature2</value>"
            "</field>"
          "</x>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(0, listener->error_count);

  std::string response_iq =
      "<iq xmlns='jabber:client' id='0' type='result'"
      "  from='muc-jid-ponies@domain.com'>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(1, listener->result_count);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomConfigTaskTest, TestError) {
  std::vector<std::string> room_features;
  buzz::MucRoomConfigTask* task = new buzz::MucRoomConfigTask(
      xmpp_client, room_jid, "ponies", room_features);
  task->SignalError.connect(listener, &MucRoomConfigListener::OnError);
  task->Start();

  std::string error_iq =
      "<iq xmlns='jabber:client' id='0' type='error'"
      " from='muc-jid-ponies@domain.com'>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(error_iq));

  EXPECT_EQ(0, listener->result_count);
  EXPECT_EQ(1, listener->error_count);
}
