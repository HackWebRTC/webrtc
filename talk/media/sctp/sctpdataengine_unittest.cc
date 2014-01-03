/*
 * libjingle SCTP
 * Copyright 2013 Google Inc
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "talk/app/webrtc/datachannelinterface.h"
#include "talk/base/bind.h"
#include "talk/base/buffer.h"
#include "talk/base/criticalsection.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/sctp/sctpdataengine.h"
#include "talk/media/sctp/sctputils.h"

enum {
  MSG_PACKET = 1,
};

// Fake NetworkInterface that sends/receives sctp packets.  The one in
// talk/media/base/fakenetworkinterface.h only works with rtp/rtcp.
class SctpFakeNetworkInterface : public cricket::MediaChannel::NetworkInterface,
                                 public talk_base::MessageHandler {
 public:
  explicit SctpFakeNetworkInterface(talk_base::Thread* thread)
    : thread_(thread),
      dest_(NULL) {
  }

  void SetDestination(cricket::DataMediaChannel* dest) { dest_ = dest; }

 protected:
  // Called to send raw packet down the wire (e.g. SCTP an packet).
  virtual bool SendPacket(talk_base::Buffer* packet,
                          talk_base::DiffServCodePoint dscp) {
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::SendPacket";

    // TODO(ldixon): Can/should we use Buffer.TransferTo here?
    // Note: this assignment does a deep copy of data from packet.
    talk_base::Buffer* buffer = new talk_base::Buffer(packet->data(),
                                                      packet->length());
    thread_->Post(this, MSG_PACKET, talk_base::WrapMessageData(buffer));
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::SendPacket, Posted message.";
    return true;
  }

  // Called when a raw packet has been recieved. This passes the data to the
  // code that will interpret the packet. e.g. to get the content payload from
  // an SCTP packet.
  virtual void OnMessage(talk_base::Message* msg) {
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::OnMessage";
    talk_base::Buffer* buffer =
        static_cast<talk_base::TypedMessageData<talk_base::Buffer*>*>(
            msg->pdata)->data();
    if (dest_) {
      dest_->OnPacketReceived(buffer, talk_base::PacketTime());
    }
    delete buffer;
  }

  // Unsupported functions required to exist by NetworkInterface.
  // TODO(ldixon): Refactor parent NetworkInterface class so these are not
  // required. They are RTC specific and should be in an appropriate subclass.
  virtual bool SendRtcp(talk_base::Buffer* packet,
                        talk_base::DiffServCodePoint dscp) {
    LOG(LS_WARNING) << "Unsupported: SctpFakeNetworkInterface::SendRtcp.";
    return false;
  }
  virtual int SetOption(SocketType type, talk_base::Socket::Option opt,
                        int option) {
    LOG(LS_WARNING) << "Unsupported: SctpFakeNetworkInterface::SetOption.";
    return 0;
  }
  virtual void SetDefaultDSCPCode(talk_base::DiffServCodePoint dscp) {
    LOG(LS_WARNING) << "Unsupported: SctpFakeNetworkInterface::SetOption.";
  }

 private:
  // Not owned by this class.
  talk_base::Thread* thread_;
  cricket::DataMediaChannel* dest_;
};

// This is essentially a buffer to hold recieved data. It stores only the last
// received data. Calling OnDataReceived twice overwrites old data with the
// newer one.
// TODO(ldixon): Implement constraints, and allow new data to be added to old
// instead of replacing it.
class SctpFakeDataReceiver : public sigslot::has_slots<> {
 public:
  SctpFakeDataReceiver() : received_(false) {}

  void Clear() {
    received_ = false;
    last_data_ = "";
    last_params_ = cricket::ReceiveDataParams();
  }

  virtual void OnDataReceived(const cricket::ReceiveDataParams& params,
                              const char* data, size_t length) {
    received_ = true;
    last_data_ = std::string(data, length);
    last_params_ = params;
  }

  bool received() const { return received_; }
  std::string last_data() const { return last_data_; }
  cricket::ReceiveDataParams last_params() const { return last_params_; }

 private:
  bool received_;
  std::string last_data_;
  cricket::ReceiveDataParams last_params_;
};

class SignalReadyToSendObserver : public sigslot::has_slots<> {
 public:
  SignalReadyToSendObserver() : signaled_(false), writable_(false) {}

  void OnSignaled(bool writable) {
    signaled_ = true;
    writable_ = writable;
  }

  bool IsSignaled(bool writable) {
    return signaled_ && (writable_ == writable);
  }

 private:
  bool signaled_;
  bool writable_;
};

class SignalChannelClosedObserver : public sigslot::has_slots<> {
 public:
  SignalChannelClosedObserver() {}
  void BindSelf(cricket::SctpDataMediaChannel* channel) {
    channel->SignalStreamClosed.connect(
        this, &SignalChannelClosedObserver::OnStreamClosed);
  }
  void OnStreamClosed(int stream) {
    streams_.push_back(stream);
  }

  int StreamCloseCount(int stream) {
    return std::count(streams_.begin(), streams_.end(), stream);
  }

  bool WasStreamClosed(int stream) {
    return std::find(streams_.begin(), streams_.end(), stream)
        != streams_.end();
  }

 private:
  std::vector<int> streams_;
};

class SignalChannelClosedReopener : public sigslot::has_slots<> {
 public:
  SignalChannelClosedReopener(cricket::SctpDataMediaChannel* channel,
                              cricket::SctpDataMediaChannel* peer)
      : channel_(channel), peer_(peer) {}

  void OnStreamClosed(int stream) {
    cricket::StreamParams p(cricket::StreamParams::CreateLegacy(stream));
    channel_->AddSendStream(p);
    channel_->AddRecvStream(p);
    peer_->AddSendStream(p);
    peer_->AddRecvStream(p);
    streams_.push_back(stream);
  }

  int StreamCloseCount(int stream) {
    return std::count(streams_.begin(), streams_.end(), stream);
  }

 private:
  cricket::SctpDataMediaChannel* channel_;
  cricket::SctpDataMediaChannel* peer_;
  std::vector<int> streams_;
};

// SCTP Data Engine testing framework.
class SctpDataMediaChannelTest : public testing::Test,
                                 public sigslot::has_slots<> {
 protected:
  virtual void SetUp() {
    engine_.reset(new cricket::SctpDataEngine());
  }

  void SetupConnectedChannels() {
    net1_.reset(new SctpFakeNetworkInterface(talk_base::Thread::Current()));
    net2_.reset(new SctpFakeNetworkInterface(talk_base::Thread::Current()));
    recv1_.reset(new SctpFakeDataReceiver());
    recv2_.reset(new SctpFakeDataReceiver());
    chan1_.reset(CreateChannel(net1_.get(), recv1_.get()));
    chan1_->set_debug_name("chan1/connector");
    chan2_.reset(CreateChannel(net2_.get(), recv2_.get()));
    chan2_->set_debug_name("chan2/listener");
    // Setup two connected channels ready to send and receive.
    net1_->SetDestination(chan2_.get());
    net2_->SetDestination(chan1_.get());

    LOG(LS_VERBOSE) << "Channel setup ----------------------------- ";
    AddStream(1);
    AddStream(2);

    LOG(LS_VERBOSE) << "Connect the channels -----------------------------";
    // chan1 wants to setup a data connection.
    chan1_->SetReceive(true);
    // chan1 will have sent chan2 a request to setup a data connection. After
    // chan2 accepts the offer, chan2 connects to chan1 with the following.
    chan2_->SetReceive(true);
    chan2_->SetSend(true);
    // Makes sure that network packets are delivered and simulates a
    // deterministic and realistic small timing delay between the SetSend calls.
    ProcessMessagesUntilIdle();

    // chan1 and chan2 are now connected so chan1 enables sending to complete
    // the creation of the connection.
    chan1_->SetSend(true);
  }

  virtual void TearDown() {
    channel1()->SetSend(false);
    channel2()->SetSend(false);
  }

  void AddStream(int ssrc) {
    cricket::StreamParams p(cricket::StreamParams::CreateLegacy(ssrc));
    chan1_->AddSendStream(p);
    chan1_->AddRecvStream(p);
    chan2_->AddSendStream(p);
    chan2_->AddRecvStream(p);
  }

  cricket::SctpDataMediaChannel* CreateChannel(
      SctpFakeNetworkInterface* net, SctpFakeDataReceiver* recv) {
    cricket::SctpDataMediaChannel* channel =
        static_cast<cricket::SctpDataMediaChannel*>(engine_->CreateChannel(
            cricket::DCT_SCTP));
    channel->SetInterface(net);
    // When data is received, pass it to the SctpFakeDataReceiver.
    channel->SignalDataReceived.connect(
        recv, &SctpFakeDataReceiver::OnDataReceived);
    channel->SignalNewStreamReceived.connect(
        this, &SctpDataMediaChannelTest::OnNewStreamReceived);
    return channel;
  }

  bool SendData(cricket::SctpDataMediaChannel* chan, uint32 ssrc,
                const std::string& msg,
                cricket::SendDataResult* result) {
    cricket::SendDataParams params;
    params.ssrc = ssrc;
    return chan->SendData(params, talk_base::Buffer(
        msg.data(), msg.length()), result);
  }

  bool ReceivedData(const SctpFakeDataReceiver* recv, uint32 ssrc,
                    const std::string& msg ) {
    return (recv->received() &&
            recv->last_params().ssrc == ssrc &&
            recv->last_data() == msg);
  }

  bool ProcessMessagesUntilIdle() {
    talk_base::Thread* thread = talk_base::Thread::Current();
    while (!thread->empty()) {
      talk_base::Message msg;
      if (thread->Get(&msg, talk_base::kForever)) {
        thread->Dispatch(&msg);
      }
    }
    return !thread->IsQuitting();
  }

  cricket::SctpDataMediaChannel* channel1() { return chan1_.get(); }
  cricket::SctpDataMediaChannel* channel2() { return chan2_.get(); }
  SctpFakeDataReceiver* receiver1() { return recv1_.get(); }
  SctpFakeDataReceiver* receiver2() { return recv2_.get(); }

  void OnNewStreamReceived(const std::string& label,
                           const webrtc::DataChannelInit& init) {
    last_label_ = label;
    last_dc_init_ = init;
  }

  std::string last_label() { return last_label_; }
  webrtc::DataChannelInit last_dc_init() { return last_dc_init_; }

 private:
  talk_base::scoped_ptr<cricket::SctpDataEngine> engine_;
  talk_base::scoped_ptr<SctpFakeNetworkInterface> net1_;
  talk_base::scoped_ptr<SctpFakeNetworkInterface> net2_;
  talk_base::scoped_ptr<SctpFakeDataReceiver> recv1_;
  talk_base::scoped_ptr<SctpFakeDataReceiver> recv2_;
  talk_base::scoped_ptr<cricket::SctpDataMediaChannel> chan1_;
  talk_base::scoped_ptr<cricket::SctpDataMediaChannel> chan2_;
  std::string last_label_;
  webrtc::DataChannelInit last_dc_init_;
};

// Verifies that SignalReadyToSend is fired.
TEST_F(SctpDataMediaChannelTest, SignalReadyToSend) {
  SetupConnectedChannels();

  SignalReadyToSendObserver signal_observer_1;
  SignalReadyToSendObserver signal_observer_2;

  channel1()->SignalReadyToSend.connect(&signal_observer_1,
                                        &SignalReadyToSendObserver::OnSignaled);
  channel2()->SignalReadyToSend.connect(&signal_observer_2,
                                        &SignalReadyToSendObserver::OnSignaled);

  cricket::SendDataResult result;
  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);
  ASSERT_TRUE(SendData(channel2(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi chan1"), 1000);

  EXPECT_TRUE_WAIT(signal_observer_1.IsSignaled(true), 1000);
  EXPECT_TRUE_WAIT(signal_observer_2.IsSignaled(true), 1000);
}

TEST_F(SctpDataMediaChannelTest, SendData) {
  SetupConnectedChannels();

  cricket::SendDataResult result;
  LOG(LS_VERBOSE) << "chan1 sending: 'hello?' -----------------------------";
  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);
  LOG(LS_VERBOSE) << "recv2.received=" << receiver2()->received()
                  << "recv2.last_params.ssrc="
                  << receiver2()->last_params().ssrc
                  << "recv2.last_params.timestamp="
                  << receiver2()->last_params().ssrc
                  << "recv2.last_params.seq_num="
                  << receiver2()->last_params().seq_num
                  << "recv2.last_data=" << receiver2()->last_data();

  LOG(LS_VERBOSE) << "chan2 sending: 'hi chan1' -----------------------------";
  ASSERT_TRUE(SendData(channel2(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi chan1"), 1000);
  LOG(LS_VERBOSE) << "recv1.received=" << receiver1()->received()
                  << "recv1.last_params.ssrc="
                  << receiver1()->last_params().ssrc
                  << "recv1.last_params.timestamp="
                  << receiver1()->last_params().ssrc
                  << "recv1.last_params.seq_num="
                  << receiver1()->last_params().seq_num
                  << "recv1.last_data=" << receiver1()->last_data();
}

TEST_F(SctpDataMediaChannelTest, SendReceiveOpenMessage) {
  SetupConnectedChannels();

  std::string label("x");
  webrtc::DataChannelInit config;
  config.id = 10;

  // Send the OPEN message on a unknown ssrc.
  channel1()->AddSendStream(cricket::StreamParams::CreateLegacy(config.id));
  cricket::SendDataParams params;
  params.ssrc = config.id;
  params.type = cricket::DMT_CONTROL;
  cricket::SendDataResult result;
  talk_base::Buffer buffer;
  ASSERT_TRUE(cricket::WriteDataChannelOpenMessage(label, config, &buffer));
  ASSERT_TRUE(channel1()->SendData(params, buffer, &result));
  // Send data on the new ssrc immediately after sending the OPEN message.
  ASSERT_TRUE(SendData(channel1(), config.id, "hi chan2", &result));

  // Verifies the received OPEN message.
  EXPECT_TRUE_WAIT(last_label() == label, 1000);
  EXPECT_EQ(config.id, last_dc_init().id);
  EXPECT_EQ(true, last_dc_init().negotiated);
  // Verifies the received data.
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), config.id, "hi chan2"), 1000);
}

TEST_F(SctpDataMediaChannelTest, ClosesRemoteStream) {
  SetupConnectedChannels();
  SignalChannelClosedObserver chan_1_sig_receiver, chan_2_sig_receiver;
  chan_1_sig_receiver.BindSelf(channel1());
  chan_2_sig_receiver.BindSelf(channel2());

  cricket::SendDataResult result;
  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);
  ASSERT_TRUE(SendData(channel2(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi chan1"), 1000);

  // Close channel 1.  Channel 2 should notify us.
  channel1()->RemoveSendStream(1);
  EXPECT_TRUE_WAIT(chan_2_sig_receiver.WasStreamClosed(1), 1000);
}

TEST_F(SctpDataMediaChannelTest, ClosesTwoRemoteStreams) {
  SetupConnectedChannels();
  AddStream(3);
  SignalChannelClosedObserver chan_1_sig_receiver, chan_2_sig_receiver;
  chan_1_sig_receiver.BindSelf(channel1());
  chan_2_sig_receiver.BindSelf(channel2());

  cricket::SendDataResult result;
  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);
  ASSERT_TRUE(SendData(channel2(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi chan1"), 1000);

  // Close two streams on one side.
  channel2()->RemoveSendStream(2);
  channel2()->RemoveSendStream(3);
  EXPECT_TRUE_WAIT(chan_1_sig_receiver.WasStreamClosed(2), 1000);
  EXPECT_TRUE_WAIT(chan_1_sig_receiver.WasStreamClosed(3), 1000);
}

TEST_F(SctpDataMediaChannelTest, ClosesStreamsOnBothSides) {
  SetupConnectedChannels();
  AddStream(3);
  AddStream(4);
  SignalChannelClosedObserver chan_1_sig_receiver, chan_2_sig_receiver;
  chan_1_sig_receiver.BindSelf(channel1());
  chan_2_sig_receiver.BindSelf(channel2());

  cricket::SendDataResult result;
  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);
  ASSERT_TRUE(SendData(channel2(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi chan1"), 1000);

  // Close one stream on channel1(), while closing three streams on
  // channel2().  They will conflict (only one side can close anything at a
  // time, apparently).  Test the resolution of the conflict.
  channel1()->RemoveSendStream(1);

  channel2()->RemoveSendStream(2);
  channel2()->RemoveSendStream(3);
  channel2()->RemoveSendStream(4);
  EXPECT_TRUE_WAIT(chan_2_sig_receiver.WasStreamClosed(1), 1000);
  EXPECT_TRUE_WAIT(chan_1_sig_receiver.WasStreamClosed(2), 1000);
  EXPECT_TRUE_WAIT(chan_1_sig_receiver.WasStreamClosed(3), 1000);
  EXPECT_TRUE_WAIT(chan_1_sig_receiver.WasStreamClosed(4), 1000);
}

TEST_F(SctpDataMediaChannelTest, ReusesAStream) {
  // Shut down channel 1, then open it up again for reuse.
  SetupConnectedChannels();
  cricket::SendDataResult result;
  SignalChannelClosedObserver chan_2_sig_receiver;
  chan_2_sig_receiver.BindSelf(channel2());

  ASSERT_TRUE(SendData(channel1(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), 1000);

  channel1()->RemoveSendStream(1);
  EXPECT_TRUE_WAIT(chan_2_sig_receiver.WasStreamClosed(1), 1000);
  // Channel 1 is gone now.

  // Create a new channel 1.
  AddStream(1);
  ASSERT_TRUE(SendData(channel1(), 1, "hi?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hi?"), 1000);
  channel1()->RemoveSendStream(1);
  EXPECT_TRUE_WAIT(chan_2_sig_receiver.StreamCloseCount(1) == 2, 1000);
}
