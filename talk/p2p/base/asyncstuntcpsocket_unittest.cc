/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/base/asyncsocket.h"
#include "talk/base/gunit.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/virtualsocketserver.h"
#include "talk/p2p/base/asyncstuntcpsocket.h"

namespace cricket {

static unsigned char kStunMessageWithZeroLength[] = {
  0x00, 0x01, 0x00, 0x00,  // length of 0 (last 2 bytes)
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};


static unsigned char kTurnChannelDataMessageWithZeroLength[] = {
  0x40, 0x00, 0x00, 0x00,  // length of 0 (last 2 bytes)
};

static unsigned char kTurnChannelDataMessage[] = {
  0x40, 0x00, 0x00, 0x10,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kStunMessageWithInvalidLength[] = {
  0x00, 0x01, 0x00, 0x10,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithInvalidLength[] = {
  0x80, 0x00, 0x00, 0x20,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithOddLength[] = {
  0x40, 0x00, 0x00, 0x05,
  0x21, 0x12, 0xA4, 0x42,
  '0',
};


static const talk_base::SocketAddress kClientAddr("11.11.11.11", 0);
static const talk_base::SocketAddress kServerAddr("22.22.22.22", 0);

class AsyncStunTCPSocketTest : public testing::Test,
                               public sigslot::has_slots<> {
 protected:
  AsyncStunTCPSocketTest()
      : vss_(new talk_base::VirtualSocketServer(NULL)),
        ss_scope_(vss_.get()) {
  }

  virtual void SetUp() {
    CreateSockets();
  }

  void CreateSockets() {
    talk_base::AsyncSocket* server = vss_->CreateAsyncSocket(
        kServerAddr.family(), SOCK_STREAM);
    server->Bind(kServerAddr);
    recv_socket_.reset(new AsyncStunTCPSocket(server, true));
    recv_socket_->SignalNewConnection.connect(
        this, &AsyncStunTCPSocketTest::OnNewConnection);

    talk_base::AsyncSocket* client = vss_->CreateAsyncSocket(
        kClientAddr.family(), SOCK_STREAM);
    send_socket_.reset(AsyncStunTCPSocket::Create(
        client, kClientAddr, recv_socket_->GetLocalAddress()));
    ASSERT_TRUE(send_socket_.get() != NULL);
    vss_->ProcessMessagesUntilIdle();
  }

  void OnReadPacket(talk_base::AsyncPacketSocket* socket, const char* data,
                    size_t len, const talk_base::SocketAddress& remote_addr,
                    const talk_base::PacketTime& packet_time) {
    recv_packets_.push_back(std::string(data, len));
  }

  void OnNewConnection(talk_base::AsyncPacketSocket* server,
                       talk_base::AsyncPacketSocket* new_socket) {
    listen_socket_.reset(new_socket);
    new_socket->SignalReadPacket.connect(
        this, &AsyncStunTCPSocketTest::OnReadPacket);
  }

  bool Send(const void* data, size_t len) {
    size_t ret = send_socket_->Send(
        reinterpret_cast<const char*>(data), len, talk_base::DSCP_NO_CHANGE);
    vss_->ProcessMessagesUntilIdle();
    return (ret == len);
  }

  bool CheckData(const void* data, int len) {
    bool ret = false;
    if (recv_packets_.size()) {
      std::string packet =  recv_packets_.front();
      recv_packets_.pop_front();
      ret = (memcmp(data, packet.c_str(), len) == 0);
    }
    return ret;
  }

  talk_base::scoped_ptr<talk_base::VirtualSocketServer> vss_;
  talk_base::SocketServerScope ss_scope_;
  talk_base::scoped_ptr<AsyncStunTCPSocket> send_socket_;
  talk_base::scoped_ptr<AsyncStunTCPSocket> recv_socket_;
  talk_base::scoped_ptr<talk_base::AsyncPacketSocket> listen_socket_;
  std::list<std::string> recv_packets_;
};

// Testing a stun packet sent/recv properly.
TEST_F(AsyncStunTCPSocketTest, TestSingleStunPacket) {
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kStunMessageWithZeroLength,
                        sizeof(kStunMessageWithZeroLength)));
}

// Verify sending multiple packets.
TEST_F(AsyncStunTCPSocketTest, TestMultipleStunPackets) {
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(4u, recv_packets_.size());
}

// Verifying TURN channel data message with zero length.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataWithZeroLength) {
  EXPECT_TRUE(Send(kTurnChannelDataMessageWithZeroLength,
                   sizeof(kTurnChannelDataMessageWithZeroLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithZeroLength,
                        sizeof(kTurnChannelDataMessageWithZeroLength)));
}

// Verifying TURN channel data message.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelData) {
  EXPECT_TRUE(Send(kTurnChannelDataMessage,
                   sizeof(kTurnChannelDataMessage)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessage,
                        sizeof(kTurnChannelDataMessage)));
}

// Verifying TURN channel messages which needs padding handled properly.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataPadding) {
  EXPECT_TRUE(Send(kTurnChannelDataMessageWithOddLength,
                   sizeof(kTurnChannelDataMessageWithOddLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithOddLength,
                        sizeof(kTurnChannelDataMessageWithOddLength)));
}

// Verifying stun message with invalid length.
TEST_F(AsyncStunTCPSocketTest, TestStunInvalidLength) {
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));
  EXPECT_EQ(0u, recv_packets_.size());

  // Modify the message length to larger value.
  kStunMessageWithInvalidLength[2] = 0xFF;
  kStunMessageWithInvalidLength[3] = 0xFF;
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));

  // Modify the message length to smaller value.
  kStunMessageWithInvalidLength[2] = 0x00;
  kStunMessageWithInvalidLength[3] = 0x01;
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));
}

// Verifying TURN channel data message with invalid length.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataWithInvalidLength) {
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));
  // Modify the length to larger value.
  kTurnChannelDataMessageWithInvalidLength[2] = 0xFF;
  kTurnChannelDataMessageWithInvalidLength[3] = 0xF0;
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));

  // Modify the length to smaller value.
  kTurnChannelDataMessageWithInvalidLength[2] = 0x00;
  kTurnChannelDataMessageWithInvalidLength[3] = 0x00;
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));
}

// Verifying a small buffer handled (dropped) properly. This will be
// a common one for both stun and turn.
TEST_F(AsyncStunTCPSocketTest, TestTooSmallMessageBuffer) {
  char data[1];
  EXPECT_FALSE(Send(data, sizeof(data)));
}

// Verifying a legal large turn message.
TEST_F(AsyncStunTCPSocketTest, TestMaximumSizeTurnPacket) {
  // We have problem in getting the SignalWriteEvent from the virtual socket
  // server. So increasing the send buffer to 64k.
  // TODO(mallinath) - Remove this setting after we fix vss issue.
  vss_->set_send_buffer_capacity(64 * 1024);
  unsigned char packet[65539];
  packet[0] = 0x40;
  packet[1] = 0x00;
  packet[2] = 0xFF;
  packet[3] = 0xFF;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Verifying a legal large stun message.
TEST_F(AsyncStunTCPSocketTest, TestMaximumSizeStunPacket) {
  // We have problem in getting the SignalWriteEvent from the virtual socket
  // server. So increasing the send buffer to 64k.
  // TODO(mallinath) - Remove this setting after we fix vss issue.
  vss_->set_send_buffer_capacity(64 * 1024);
  unsigned char packet[65552];
  packet[0] = 0x00;
  packet[1] = 0x01;
  packet[2] = 0xFF;
  packet[3] = 0xFC;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Investigate why WriteEvent is not signaled from VSS.
TEST_F(AsyncStunTCPSocketTest, DISABLED_TestWithSmallSendBuffer) {
  vss_->set_send_buffer_capacity(1);
  Send(kTurnChannelDataMessageWithOddLength,
       sizeof(kTurnChannelDataMessageWithOddLength));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithOddLength,
                        sizeof(kTurnChannelDataMessageWithOddLength)));
}

}  // namespace cricket
