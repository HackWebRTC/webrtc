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
#include "talk/xmpp/mucroomlookuptask.h"

class MucRoomLookupListener : public sigslot::has_slots<> {
 public:
  MucRoomLookupListener() : error_count(0) {}

  void OnResult(buzz::MucRoomLookupTask* task,
                const buzz::MucRoomInfo& room) {
    last_room = room;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  buzz::MucRoomInfo last_room;
  int error_count;
};

class MucRoomLookupTaskTest : public testing::Test {
 public:
  MucRoomLookupTaskTest() :
      lookup_server_jid("lookup@domain.com"),
      room_jid("muc-jid-ponies@domain.com"),
      room_name("ponies"),
      room_domain("domain.com"),
      room_full_name("ponies@domain.com"),
      hangout_id("some_hangout_id") {
  }

  virtual void SetUp() {
    runner = new talk_base::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomLookupListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  talk_base::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomLookupListener* listener;
  buzz::Jid lookup_server_jid;
  buzz::Jid room_jid;
  std::string room_name;
  std::string room_domain;
  std::string room_full_name;
  std::string hangout_id;
};

TEST_F(MucRoomLookupTaskTest, TestLookupName) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomLookupTask* task =
      buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
          xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalResult.connect(listener, &MucRoomLookupListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"lookup@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"jabber:iq:search\">"
          "<room-name>ponies</room-name>"
          "<room-domain>domain.com</room-domain>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_room.name);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item jid='muc-jid-ponies@domain.com'>"
      "      <room-name>ponies</room-name>"
      "      <room-domain>domain.com</room-domain>"
      "    </item>"
      "  </query>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(room_name, listener->last_room.name);
  EXPECT_EQ(room_domain, listener->last_room.domain);
  EXPECT_EQ(room_jid, listener->last_room.jid);
  EXPECT_EQ(room_full_name, listener->last_room.full_name());
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestLookupHangoutId) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForHangoutId(
      xmpp_client, lookup_server_jid, hangout_id);
  task->SignalResult.connect(listener, &MucRoomLookupListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"set\" to=\"lookup@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<query xmlns=\"jabber:iq:search\">"
          "<hangout-id>some_hangout_id</hangout-id>"
        "</query>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_room.name);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item jid='muc-jid-ponies@domain.com'>"
      "      <room-name>some_hangout_id</room-name>"
      "      <room-domain>domain.com</room-domain>"
      "    </item>"
      "  </query>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(hangout_id, listener->last_room.name);
  EXPECT_EQ(room_domain, listener->last_room.domain);
  EXPECT_EQ(room_jid, listener->last_room.jid);
  EXPECT_EQ(0, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestError) {
  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
      xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalError.connect(listener, &MucRoomLookupListener::OnError);
  task->Start();

  std::string error_iq =
      "<iq xmlns='jabber:client' id='0' type='error'"
      "  from='lookup@domain.com'>"
      "</iq>";

  EXPECT_EQ(0, listener->error_count);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(error_iq));
  EXPECT_EQ(1, listener->error_count);
}

TEST_F(MucRoomLookupTaskTest, TestBadJid) {
  buzz::MucRoomLookupTask* task = buzz::MucRoomLookupTask::CreateLookupTaskForRoomName(
      xmpp_client, lookup_server_jid, room_name, room_domain);
  task->SignalError.connect(listener, &MucRoomLookupListener::OnError);
  task->Start();

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
      "  <query xmlns='jabber:iq:search'>"
      "    <item/>"
      "  </query>"
      "</iq>";

  EXPECT_EQ(0, listener->error_count);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));
  EXPECT_EQ(1, listener->error_count);
}
