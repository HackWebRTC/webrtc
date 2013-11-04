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
#include "talk/app/webrtc/test/fakedatachannelprovider.h"
#include "talk/base/gunit.h"

using webrtc::DataChannel;

class SctpDataChannelTest : public testing::Test {
 protected:
  SctpDataChannelTest()
      : webrtc_data_channel_(
          DataChannel::Create(&provider_, cricket::DCT_SCTP, "test", &init_)) {
  }

  void SetChannelReady() {
    provider_.set_transport_available(true);
    webrtc_data_channel_->OnTransportChannelCreated();
    if (webrtc_data_channel_->id() < 0) {
      webrtc_data_channel_->SetSctpSid(0);
    }
    provider_.set_ready_to_send(true);
  }

  webrtc::DataChannelInit init_;
  FakeDataChannelProvider provider_;
  talk_base::scoped_refptr<DataChannel> webrtc_data_channel_;
};

// Verifies that the data channel is connected to the transport after creation.
TEST_F(SctpDataChannelTest, ConnectedToTransportOnCreated) {
  provider_.set_transport_available(true);
  talk_base::scoped_refptr<DataChannel> dc = DataChannel::Create(
      &provider_, cricket::DCT_SCTP, "test1", &init_);

  EXPECT_TRUE(provider_.IsConnected(dc.get()));
  // The sid is not set yet, so it should not have added the streams.
  EXPECT_FALSE(provider_.IsSendStreamAdded(dc->id()));
  EXPECT_FALSE(provider_.IsRecvStreamAdded(dc->id()));

  dc->SetSctpSid(0);
  EXPECT_TRUE(provider_.IsSendStreamAdded(dc->id()));
  EXPECT_TRUE(provider_.IsRecvStreamAdded(dc->id()));
}

// Verifies that the data channel is connected to the transport if the transport
// is not available initially and becomes available later.
TEST_F(SctpDataChannelTest, ConnectedAfterTransportBecomesAvailable) {
  EXPECT_FALSE(provider_.IsConnected(webrtc_data_channel_.get()));

  provider_.set_transport_available(true);
  webrtc_data_channel_->OnTransportChannelCreated();
  EXPECT_TRUE(provider_.IsConnected(webrtc_data_channel_.get()));
}

// Tests the state of the data channel.
TEST_F(SctpDataChannelTest, StateTransition) {
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            webrtc_data_channel_->state());
  SetChannelReady();

  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, webrtc_data_channel_->state());
  webrtc_data_channel_->Close();
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed,
            webrtc_data_channel_->state());
  // Verifies that it's disconnected from the transport.
  EXPECT_FALSE(provider_.IsConnected(webrtc_data_channel_.get()));
}

// Tests that DataChannel::buffered_amount() is correct after the channel is
// blocked.
TEST_F(SctpDataChannelTest, BufferedAmountWhenBlocked) {
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());

  provider_.set_send_blocked(true);

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
  provider_.set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  provider_.set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
}

// Tests that the queued control message is sent when channel is ready.
TEST_F(SctpDataChannelTest, OpenMessageSent) {
  // Initially the id is unassigned.
  EXPECT_EQ(-1, webrtc_data_channel_->id());

  SetChannelReady();
  EXPECT_GE(webrtc_data_channel_->id(), 0);
  EXPECT_EQ(cricket::DMT_CONTROL, provider_.last_send_data_params().type);
  EXPECT_EQ(provider_.last_send_data_params().ssrc,
            static_cast<uint32>(webrtc_data_channel_->id()));
}

// Tests that the DataChannel created after transport gets ready can enter OPEN
// state.
TEST_F(SctpDataChannelTest, LateCreatedChannelTransitionToOpen) {
  SetChannelReady();
  webrtc::DataChannelInit init;
  init.id = 1;
  talk_base::scoped_refptr<DataChannel> dc =
      DataChannel::Create(&provider_, cricket::DCT_SCTP, "test1", &init);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, dc->state());
  EXPECT_TRUE_WAIT(webrtc::DataChannelInterface::kOpen == dc->state(),
                   1000);
}
