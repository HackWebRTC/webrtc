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
#include "talk/xmpp/pingtask.h"

class PingTaskTest;

class PingXmppClient : public buzz::FakeXmppClient {
 public:
  PingXmppClient(talk_base::TaskParent* parent, PingTaskTest* tst) :
      FakeXmppClient(parent), test(tst) {
  }

  buzz::XmppReturnStatus SendStanza(const buzz::XmlElement* stanza);

 private:
  PingTaskTest* test;
};

class PingTaskTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PingTaskTest() : respond_to_pings(true), timed_out(false) {
  }

  virtual void SetUp() {
    runner = new talk_base::FakeTaskRunner();
    xmpp_client = new PingXmppClient(runner, this);
  }

  virtual void TearDown() {
    // delete xmpp_client;  Deleted by deleting runner.
    delete runner;
  }

  void ConnectTimeoutSignal(buzz::PingTask* task) {
    task->SignalTimeout.connect(this, &PingTaskTest::OnPingTimeout);
  }

  void OnPingTimeout() {
    timed_out = true;
  }

  talk_base::FakeTaskRunner* runner;
  PingXmppClient* xmpp_client;
  bool respond_to_pings;
  bool timed_out;
};

buzz::XmppReturnStatus PingXmppClient::SendStanza(
    const buzz::XmlElement* stanza) {
  buzz::XmppReturnStatus result = FakeXmppClient::SendStanza(stanza);
  if (test->respond_to_pings && (stanza->FirstNamed(buzz::QN_PING) != NULL)) {
    std::string ping_response =
        "<iq xmlns=\'jabber:client\' id='0' type='result'/>";
    HandleStanza(buzz::XmlElement::ForStr(ping_response));
  }
  return result;
}

TEST_F(PingTaskTest, TestSuccess) {
  uint32 ping_period_millis = 100;
  buzz::PingTask* task = new buzz::PingTask(xmpp_client,
      talk_base::Thread::Current(),
      ping_period_millis, ping_period_millis / 10);
  ConnectTimeoutSignal(task);
  task->Start();
  unsigned int expected_ping_count = 5U;
  EXPECT_EQ_WAIT(xmpp_client->sent_stanzas().size(), expected_ping_count,
                 ping_period_millis * (expected_ping_count + 1));
  EXPECT_FALSE(task->IsDone());
  EXPECT_FALSE(timed_out);
}

TEST_F(PingTaskTest, TestTimeout) {
  respond_to_pings = false;
  uint32 ping_timeout_millis = 200;
  buzz::PingTask* task = new buzz::PingTask(xmpp_client,
      talk_base::Thread::Current(),
      ping_timeout_millis * 10, ping_timeout_millis);
  ConnectTimeoutSignal(task);
  task->Start();
  WAIT(false, ping_timeout_millis / 2);
  EXPECT_FALSE(timed_out);
  EXPECT_TRUE_WAIT(timed_out, ping_timeout_millis * 2);
}
