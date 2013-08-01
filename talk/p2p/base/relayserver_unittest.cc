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
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/socketaddress.h"
#include "talk/base/testclient.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/relayserver.h"

using talk_base::SocketAddress;
using namespace cricket;

static const uint32 LIFETIME = 4;  // seconds
static const SocketAddress server_int_addr("127.0.0.1", 5000);
static const SocketAddress server_ext_addr("127.0.0.1", 5001);
static const SocketAddress client1_addr("127.0.0.1", 6000 + (rand() % 1000));
static const SocketAddress client2_addr("127.0.0.1", 7000 + (rand() % 1000));
static const char* bad = "this is a completely nonsensical message whose only "
                         "purpose is to make the parser go 'ack'.  it doesn't "
                         "look anything like a normal stun message";
static const char* msg1 = "spamspamspamspamspamspamspambakedbeansspam";
static const char* msg2 = "Lobster Thermidor a Crevette with a mornay sauce...";

class RelayServerTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    talk_base::InitRandom(NULL, 0);
  }
  RelayServerTest()
      : main_(talk_base::Thread::Current()), ss_(main_->socketserver()),
        username_(talk_base::CreateRandomString(12)),
        password_(talk_base::CreateRandomString(12)) {
  }
 protected:
  virtual void SetUp() {
    server_.reset(new RelayServer(main_));

    server_->AddInternalSocket(
        talk_base::AsyncUDPSocket::Create(ss_, server_int_addr));
    server_->AddExternalSocket(
        talk_base::AsyncUDPSocket::Create(ss_, server_ext_addr));

    client1_.reset(new talk_base::TestClient(
        talk_base::AsyncUDPSocket::Create(ss_, client1_addr)));
    client2_.reset(new talk_base::TestClient(
        talk_base::AsyncUDPSocket::Create(ss_, client2_addr)));
  }

  void Allocate() {
    talk_base::scoped_ptr<StunMessage> req(
        CreateStunMessage(STUN_ALLOCATE_REQUEST));
    AddUsernameAttr(req.get(), username_);
    AddLifetimeAttr(req.get(), LIFETIME);
    Send1(req.get());
    delete Receive1();
  }
  void Bind() {
    talk_base::scoped_ptr<StunMessage> req(
        CreateStunMessage(STUN_BINDING_REQUEST));
    AddUsernameAttr(req.get(), username_);
    Send2(req.get());
    delete Receive1();
  }

  void Send1(const StunMessage* msg) {
    talk_base::ByteBuffer buf;
    msg->Write(&buf);
    SendRaw1(buf.Data(), static_cast<int>(buf.Length()));
  }
  void Send2(const StunMessage* msg) {
    talk_base::ByteBuffer buf;
    msg->Write(&buf);
    SendRaw2(buf.Data(), static_cast<int>(buf.Length()));
  }
  void SendRaw1(const char* data, int len) {
    return Send(client1_.get(), data, len, server_int_addr);
  }
  void SendRaw2(const char* data, int len) {
    return Send(client2_.get(), data, len, server_ext_addr);
  }
  void Send(talk_base::TestClient* client, const char* data,
            int len, const SocketAddress& addr) {
    client->SendTo(data, len, addr);
  }

  StunMessage* Receive1() {
    return Receive(client1_.get());
  }
  StunMessage* Receive2() {
    return Receive(client2_.get());
  }
  std::string ReceiveRaw1() {
    return ReceiveRaw(client1_.get());
  }
  std::string ReceiveRaw2() {
    return ReceiveRaw(client2_.get());
  }
  StunMessage* Receive(talk_base::TestClient* client) {
    StunMessage* msg = NULL;
    talk_base::TestClient::Packet* packet = client->NextPacket();
    if (packet) {
      talk_base::ByteBuffer buf(packet->buf, packet->size);
      msg = new RelayMessage();
      msg->Read(&buf);
      delete packet;
    }
    return msg;
  }
  std::string ReceiveRaw(talk_base::TestClient* client) {
    std::string raw;
    talk_base::TestClient::Packet* packet = client->NextPacket();
    if (packet) {
      raw = std::string(packet->buf, packet->size);
      delete packet;
    }
    return raw;
  }

  static StunMessage* CreateStunMessage(int type) {
    StunMessage* msg = new RelayMessage();
    msg->SetType(type);
    msg->SetTransactionID(
        talk_base::CreateRandomString(kStunTransactionIdLength));
    return msg;
  }
  static void AddMagicCookieAttr(StunMessage* msg) {
    StunByteStringAttribute* attr =
        StunAttribute::CreateByteString(STUN_ATTR_MAGIC_COOKIE);
    attr->CopyBytes(TURN_MAGIC_COOKIE_VALUE, sizeof(TURN_MAGIC_COOKIE_VALUE));
    msg->AddAttribute(attr);
  }
  static void AddUsernameAttr(StunMessage* msg, const std::string& val) {
    StunByteStringAttribute* attr =
        StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
    attr->CopyBytes(val.c_str(), val.size());
    msg->AddAttribute(attr);
  }
  static void AddLifetimeAttr(StunMessage* msg, int val) {
    StunUInt32Attribute* attr =
        StunAttribute::CreateUInt32(STUN_ATTR_LIFETIME);
    attr->SetValue(val);
    msg->AddAttribute(attr);
  }
  static void AddDestinationAttr(StunMessage* msg, const SocketAddress& addr) {
    StunAddressAttribute* attr =
        StunAttribute::CreateAddress(STUN_ATTR_DESTINATION_ADDRESS);
    attr->SetIP(addr.ipaddr());
    attr->SetPort(addr.port());
    msg->AddAttribute(attr);
  }

  talk_base::Thread* main_;
  talk_base::SocketServer* ss_;
  talk_base::scoped_ptr<RelayServer> server_;
  talk_base::scoped_ptr<talk_base::TestClient> client1_;
  talk_base::scoped_ptr<talk_base::TestClient> client2_;
  std::string username_;
  std::string password_;
};

// Send a complete nonsense message and verify that it is eaten.
TEST_F(RelayServerTest, TestBadRequest) {
  talk_base::scoped_ptr<StunMessage> res;

  SendRaw1(bad, static_cast<int>(std::strlen(bad)));
  res.reset(Receive1());

  ASSERT_TRUE(!res);
}

// Send an allocate request without a username and verify it is rejected.
TEST_F(RelayServerTest, TestAllocateNoUsername) {
  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_ALLOCATE_REQUEST)), res;

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(32, err->number());
  EXPECT_EQ("Missing Username", err->reason());
}

// Send a binding request and verify that it is rejected.
TEST_F(RelayServerTest, TestBindingRequest) {
  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_BINDING_REQUEST)), res;
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());
}

// Send an allocate request and verify that it is accepted.
TEST_F(RelayServerTest, TestAllocate) {
  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_ALLOCATE_REQUEST)), res;
  AddUsernameAttr(req.get(), username_);
  AddLifetimeAttr(req.get(), LIFETIME);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunAddressAttribute* mapped_addr =
      res->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  ASSERT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(server_ext_addr.port(), mapped_addr->port());
  EXPECT_EQ(server_ext_addr.ipaddr(), mapped_addr->ipaddr());

  const StunUInt32Attribute* res_lifetime_attr =
      res->GetUInt32(STUN_ATTR_LIFETIME);
  ASSERT_TRUE(res_lifetime_attr != NULL);
  EXPECT_EQ(LIFETIME, res_lifetime_attr->value());
}

// Send a second allocate request and verify that it is also accepted, though
// the lifetime should be ignored.
TEST_F(RelayServerTest, TestReallocate) {
  Allocate();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_ALLOCATE_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunAddressAttribute* mapped_addr =
      res->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  ASSERT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(server_ext_addr.port(), mapped_addr->port());
  EXPECT_EQ(server_ext_addr.ipaddr(), mapped_addr->ipaddr());

  const StunUInt32Attribute* lifetime_attr =
      res->GetUInt32(STUN_ATTR_LIFETIME);
  ASSERT_TRUE(lifetime_attr != NULL);
  EXPECT_EQ(LIFETIME, lifetime_attr->value());
}

// Send a request from another client and see that it arrives at the first
// client in the binding.
TEST_F(RelayServerTest, TestRemoteBind) {
  Allocate();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_BINDING_REQUEST)), res;
  AddUsernameAttr(req.get(), username_);

  Send2(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_DATA_INDICATION, res->type());

  const StunByteStringAttribute* recv_data =
      res->GetByteString(STUN_ATTR_DATA);
  ASSERT_TRUE(recv_data != NULL);

  talk_base::ByteBuffer buf(recv_data->bytes(), recv_data->length());
  talk_base::scoped_ptr<StunMessage> res2(new StunMessage());
  EXPECT_TRUE(res2->Read(&buf));
  EXPECT_EQ(STUN_BINDING_REQUEST, res2->type());
  EXPECT_EQ(req->transaction_id(), res2->transaction_id());

  const StunAddressAttribute* src_addr =
      res->GetAddress(STUN_ATTR_SOURCE_ADDRESS2);
  ASSERT_TRUE(src_addr != NULL);
  EXPECT_EQ(1, src_addr->family());
  EXPECT_EQ(client2_addr.ipaddr(), src_addr->ipaddr());
  EXPECT_EQ(client2_addr.port(), src_addr->port());

  EXPECT_TRUE(Receive2() == NULL);
}

// Send a complete nonsense message to the established connection and verify
// that it is dropped by the server.
TEST_F(RelayServerTest, TestRemoteBadRequest) {
  Allocate();
  Bind();

  SendRaw1(bad, static_cast<int>(std::strlen(bad)));
  EXPECT_TRUE(Receive1() == NULL);
  EXPECT_TRUE(Receive2() == NULL);
}

// Send a send request without a username and verify it is rejected.
TEST_F(RelayServerTest, TestSendRequestMissingUsername) {
  Allocate();
  Bind();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(32, err->number());
  EXPECT_EQ("Missing Username", err->reason());
}

// Send a send request with the wrong username and verify it is rejected.
TEST_F(RelayServerTest, TestSendRequestBadUsername) {
  Allocate();
  Bind();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), "foobarbizbaz");

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(30, err->number());
  EXPECT_EQ("Stale Credentials", err->reason());
}

// Send a send request without a destination address and verify that it is
// rejected.
TEST_F(RelayServerTest, TestSendRequestNoDestinationAddress) {
  Allocate();
  Bind();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Bad Request", err->reason());
}

// Send a send request without data and verify that it is rejected.
TEST_F(RelayServerTest, TestSendRequestNoData) {
  Allocate();
  Bind();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);
  AddDestinationAttr(req.get(), client2_addr);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(00, err->number());
  EXPECT_EQ("Bad Request", err->reason());
}

// Send a binding request after an allocate and verify that it is rejected.
TEST_F(RelayServerTest, TestSendRequestWrongType) {
  Allocate();
  Bind();

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_BINDING_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());
}

// Verify that we can send traffic back and forth between the clients after a
// successful allocate and bind.
TEST_F(RelayServerTest, TestSendRaw) {
  Allocate();
  Bind();

  for (int i = 0; i < 10; i++) {
    talk_base::scoped_ptr<StunMessage> req(
        CreateStunMessage(STUN_SEND_REQUEST)), res;
    AddMagicCookieAttr(req.get());
    AddUsernameAttr(req.get(), username_);
    AddDestinationAttr(req.get(), client2_addr);

    StunByteStringAttribute* send_data =
        StunAttribute::CreateByteString(STUN_ATTR_DATA);
    send_data->CopyBytes(msg1);
    req->AddAttribute(send_data);

    Send1(req.get());
    EXPECT_EQ(msg1, ReceiveRaw2());
    SendRaw2(msg2, static_cast<int>(std::strlen(msg2)));
    res.reset(Receive1());

    ASSERT_TRUE(res);
    EXPECT_EQ(STUN_DATA_INDICATION, res->type());

    const StunAddressAttribute* src_addr =
        res->GetAddress(STUN_ATTR_SOURCE_ADDRESS2);
    ASSERT_TRUE(src_addr != NULL);
    EXPECT_EQ(1, src_addr->family());
    EXPECT_EQ(client2_addr.ipaddr(), src_addr->ipaddr());
    EXPECT_EQ(client2_addr.port(), src_addr->port());

    const StunByteStringAttribute* recv_data =
        res->GetByteString(STUN_ATTR_DATA);
    ASSERT_TRUE(recv_data != NULL);
    EXPECT_EQ(strlen(msg2), recv_data->length());
    EXPECT_EQ(0, memcmp(msg2, recv_data->bytes(), recv_data->length()));
  }
}

// Verify that a binding expires properly, and rejects send requests.
TEST_F(RelayServerTest, TestExpiration) {
  Allocate();
  Bind();

  // Wait twice the lifetime to make sure the server has expired the binding.
  talk_base::Thread::Current()->ProcessMessages((LIFETIME * 2) * 1000);

  talk_base::scoped_ptr<StunMessage> req(
      CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);
  AddDestinationAttr(req.get(), client2_addr);

  StunByteStringAttribute* data_attr =
      StunAttribute::CreateByteString(STUN_ATTR_DATA);
  data_attr->CopyBytes(msg1);
  req->AddAttribute(data_attr);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res.get() != NULL);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());

  // Also verify that traffic from the external client is ignored.
  SendRaw2(msg2, static_cast<int>(std::strlen(msg2)));
  EXPECT_TRUE(ReceiveRaw1().empty());
}
