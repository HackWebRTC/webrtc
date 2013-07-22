/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/virtualsocketserver.h"
#include "talk/base/testclient.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/stunserver.h"

using namespace cricket;

static const talk_base::SocketAddress server_addr("99.99.99.1", 3478);
static const talk_base::SocketAddress client_addr("1.2.3.4", 1234);

class StunServerTest : public testing::Test {
 public:
  StunServerTest()
    : pss_(new talk_base::PhysicalSocketServer),
      ss_(new talk_base::VirtualSocketServer(pss_.get())),
      worker_(ss_.get()) {
  }
  virtual void SetUp() {
    server_.reset(new StunServer(
        talk_base::AsyncUDPSocket::Create(ss_.get(), server_addr)));
    client_.reset(new talk_base::TestClient(
        talk_base::AsyncUDPSocket::Create(ss_.get(), client_addr)));

    worker_.Start();
  }
  void Send(const StunMessage& msg) {
    talk_base::ByteBuffer buf;
    msg.Write(&buf);
    Send(buf.Data(), static_cast<int>(buf.Length()));
  }
  void Send(const char* buf, int len) {
    client_->SendTo(buf, len, server_addr);
  }
  StunMessage* Receive() {
    StunMessage* msg = NULL;
    talk_base::TestClient::Packet* packet = client_->NextPacket();
    if (packet) {
      talk_base::ByteBuffer buf(packet->buf, packet->size);
      msg = new StunMessage();
      msg->Read(&buf);
      delete packet;
    }
    return msg;
  }
 private:
  talk_base::scoped_ptr<talk_base::PhysicalSocketServer> pss_;
  talk_base::scoped_ptr<talk_base::VirtualSocketServer> ss_;
  talk_base::Thread worker_;
  talk_base::scoped_ptr<StunServer> server_;
  talk_base::scoped_ptr<talk_base::TestClient> client_;
};

TEST_F(StunServerTest, TestGood) {
  StunMessage req;
  std::string transaction_id = "0123456789ab";
  req.SetType(STUN_BINDING_REQUEST);
  req.SetTransactionID(transaction_id);
  Send(req);

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_EQ(req.transaction_id(), msg->transaction_id());

  const StunAddressAttribute* mapped_addr =
      msg->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  EXPECT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(client_addr.port(), mapped_addr->port());
  if (mapped_addr->ipaddr() != client_addr.ipaddr()) {
    LOG(LS_WARNING) << "Warning: mapped IP ("
                    << mapped_addr->ipaddr()
                    << ") != local IP (" << client_addr.ipaddr()
                    << ")";
  }

  delete msg;
}

TEST_F(StunServerTest, TestBad) {
  const char* bad = "this is a completely nonsensical message whose only "
                    "purpose is to make the parser go 'ack'.  it doesn't "
                    "look anything like a normal stun message";
  Send(bad, static_cast<int>(std::strlen(bad)));

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg == NULL);
}
