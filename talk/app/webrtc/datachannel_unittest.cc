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
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/session/media/channelmanager.h"

using webrtc::MediaConstraintsInterface;

const uint32 kFakeSsrc = 1;

class SctpDataChannelTest : public testing::Test {
 protected:
  SctpDataChannelTest()
      : media_engine_(new cricket::FakeMediaEngine),
        data_engine_(new cricket::FakeDataEngine),
        channel_manager_(
            new cricket::ChannelManager(media_engine_,
                                        data_engine_,
                                        new cricket::FakeDeviceManager(),
                                        new cricket::CaptureManager(),
                                        talk_base::Thread::Current())),
        ms_signaling_(new webrtc::MediaStreamSignaling(
                          talk_base::Thread::Current(), NULL)),
        session_(channel_manager_.get(),
                 talk_base::Thread::Current(),
                 talk_base::Thread::Current(),
                 NULL,
                 ms_signaling_.get()),
        webrtc_data_channel_(NULL) {}

  virtual void SetUp() {
    if (!talk_base::SSLStreamAdapter::HaveDtlsSrtp()) {
      return;
    }
    channel_manager_->Init();
    webrtc::FakeConstraints constraints;
    constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp, true);
    constraints.AddMandatory(MediaConstraintsInterface::kEnableSctpDataChannels,
                             true);
    ASSERT_TRUE(session_.Initialize(&constraints));
    webrtc::SessionDescriptionInterface* offer = session_.CreateOffer(NULL);
    ASSERT_TRUE(offer != NULL);
    ASSERT_TRUE(session_.SetLocalDescription(offer, NULL));

    webrtc_data_channel_ = webrtc::DataChannel::Create(&session_, "test", NULL);
    // Connect to the media channel.
    webrtc_data_channel_->SetSendSsrc(kFakeSsrc);
    webrtc_data_channel_->SetReceiveSsrc(kFakeSsrc);

    session_.data_channel()->SignalReadyToSendData(true);
  }

  void SetSendBlocked(bool blocked) {
    bool was_blocked = data_engine_->GetChannel(0)->is_send_blocked();
    data_engine_->GetChannel(0)->set_send_blocked(blocked);
    if (!blocked && was_blocked) {
      session_.data_channel()->SignalReadyToSendData(true);
    }
  }

  cricket::FakeMediaEngine* media_engine_;
  cricket::FakeDataEngine* data_engine_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  talk_base::scoped_ptr<webrtc::MediaStreamSignaling> ms_signaling_;
  webrtc::WebRtcSession session_;
  talk_base::scoped_refptr<webrtc::DataChannel> webrtc_data_channel_;
};

// Tests that DataChannel::buffered_amount() is correct after the channel is
// blocked.
TEST_F(SctpDataChannelTest, BufferedAmountWhenBlocked) {
  if (!talk_base::SSLStreamAdapter::HaveDtlsSrtp()) {
    return;
  }
  webrtc::DataBuffer buffer("abcd");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());

  SetSendBlocked(true);
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
  if (!talk_base::SSLStreamAdapter::HaveDtlsSrtp()) {
    return;
  }
  webrtc::DataBuffer buffer("abcd");
  SetSendBlocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  SetSendBlocked(false);
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
}
