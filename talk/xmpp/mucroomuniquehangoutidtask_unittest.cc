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
#include "talk/xmpp/mucroomuniquehangoutidtask.h"

class MucRoomUniqueHangoutIdListener : public sigslot::has_slots<> {
 public:
  MucRoomUniqueHangoutIdListener() : error_count(0) {}

  void OnResult(buzz::MucRoomUniqueHangoutIdTask* task,
                const std::string& hangout_id) {
    last_hangout_id = hangout_id;
  }

  void OnError(buzz::IqTask* task,
               const buzz::XmlElement* error) {
    ++error_count;
  }

  std::string last_hangout_id;
  int error_count;
};

class MucRoomUniqueHangoutIdTaskTest : public testing::Test {
 public:
  MucRoomUniqueHangoutIdTaskTest() :
      lookup_server_jid("lookup@domain.com"),
      hangout_id("some_hangout_id") {
  }

  virtual void SetUp() {
    runner = new talk_base::FakeTaskRunner();
    xmpp_client = new buzz::FakeXmppClient(runner);
    listener = new MucRoomUniqueHangoutIdListener();
  }

  virtual void TearDown() {
    delete listener;
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  talk_base::FakeTaskRunner* runner;
  buzz::FakeXmppClient* xmpp_client;
  MucRoomUniqueHangoutIdListener* listener;
  buzz::Jid lookup_server_jid;
  std::string hangout_id;
};

TEST_F(MucRoomUniqueHangoutIdTaskTest, Test) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  buzz::MucRoomUniqueHangoutIdTask* task = new buzz::MucRoomUniqueHangoutIdTask(
      xmpp_client, lookup_server_jid);
  task->SignalResult.connect(listener, &MucRoomUniqueHangoutIdListener::OnResult);
  task->Start();

  std::string expected_iq =
      "<cli:iq type=\"get\" to=\"lookup@domain.com\" id=\"0\" "
          "xmlns:cli=\"jabber:client\">"
        "<uni:unique hangout-id=\"true\" "
          "xmlns:uni=\"http://jabber.org/protocol/muc#unique\"/>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_iq, xmpp_client->sent_stanzas()[0]->Str());

  EXPECT_EQ("", listener->last_hangout_id);

  std::string response_iq =
      "<iq xmlns='jabber:client' from='lookup@domain.com' id='0' type='result'>"
        "<unique hangout-id=\"some_hangout_id\" "
            "xmlns=\"http://jabber.org/protocol/muc#unique\">"
          "muvc-private-chat-00001234-5678-9abc-def0-123456789abc"
        "</unique>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(response_iq));

  EXPECT_EQ(hangout_id, listener->last_hangout_id);
  EXPECT_EQ(0, listener->error_count);
}

