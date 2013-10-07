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

#include "talk/app/webrtc/datachannel.h"
#include "talk/base/gunit.h"

using webrtc::DataChannel;

class FakeDataChannelClient : public webrtc::DataChannelProviderInterface {
 public:
  FakeDataChannelClient() : send_blocked_(false) {}
  virtual ~FakeDataChannelClient() {}

  virtual bool SendData(const cricket::SendDataParams& params,
                        const talk_base::Buffer& payload,
                        cricket::SendDataResult* result) OVERRIDE {
    if (send_blocked_) {
      *result = cricket::SDR_BLOCK;
      return false;
    }
    last_send_data_params_ = params;
    return true;
  }
  virtual bool ConnectDataChannel(DataChannel* data_channel) OVERRIDE {
    return true;
  }
  virtual void DisconnectDataChannel(DataChannel* data_channel) OVERRIDE {}

  void set_send_blocked(bool blocked) { send_blocked_ = blocked; }
  cricket::SendDataParams last_send_data_params() {
      return last_send_data_params_;
  }

 private:
  cricket::SendDataParams last_send_data_params_;
  bool send_blocked_;
};

class SctpDataChannelTest : public testing::Test {
 protected:
  SctpDataChannelTest()
      : webrtc_data_channel_(
          DataChannel::Create(&client_, cricket::DCT_SCTP, "test", &init_)) {
  }

  void SetChannelReady() {
    webrtc_data_channel_->OnChannelReady(true);
  }

  webrtc::DataChannelInit init_;
  FakeDataChannelClient client_;
  talk_base::scoped_refptr<DataChannel> webrtc_data_channel_;
};

// Tests the state of the data channel.
TEST_F(SctpDataChannelTest, StateTransition) {
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            webrtc_data_channel_->state());
  SetChannelReady();
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, webrtc_data_channel_->state());
  webrtc_data_channel_->Close();
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed,
            webrtc_data_channel_->state());
}

// Tests that DataChannel::buffered_amount() is correct after the channel is
// blocked.
TEST_F(SctpDataChannelTest, BufferedAmountWhenBlocked) {
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());

  client_.set_send_blocked(true);

  const int number_of_packets = 3;
  for (int i = 0; i < number_of_packets; ++i) {
    EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  }
  EXPECT_EQ(buffer.data.length() * number_of_packets,
            webrtc_data_channel_->buffered_amount());
}

// Tests that the queued data are sent when the channel transitions from blocked
// to unblocked.
TEST_F(SctpDataChannelTest, QueuedDataSentWhenUnblocked) {
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  client_.set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  client_.set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
}

// Tests that the queued control message is sent when channel is ready.
TEST_F(SctpDataChannelTest, QueuedControlMessageSent) {
  talk_base::Buffer* buffer = new talk_base::Buffer("abcd", 4);
  EXPECT_TRUE(webrtc_data_channel_->SendControl(buffer));
  SetChannelReady();
  EXPECT_EQ(cricket::DMT_CONTROL, client_.last_send_data_params().type);
}
