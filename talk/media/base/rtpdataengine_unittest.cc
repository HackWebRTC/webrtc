/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/base/buffer.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/timing.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/rtpdataengine.h"
#include "talk/media/base/rtputils.h"

class FakeTiming : public talk_base::Timing {
 public:
  FakeTiming() : now_(0.0) {}

  virtual double TimerNow() {
    return now_;
  }

  void set_now(double now) {
    now_ = now;
  }

 private:
  double now_;
};

class FakeDataReceiver : public sigslot::has_slots<> {
 public:
  FakeDataReceiver() : has_received_data_(false) {}

  void OnDataReceived(
      const cricket::ReceiveDataParams& params,
      const char* data, size_t len) {
    has_received_data_ = true;
    last_received_data_ = std::string(data, len);
    last_received_data_len_ = len;
    last_received_data_params_ = params;
  }

  bool has_received_data() const { return has_received_data_; }
  std::string last_received_data() const { return last_received_data_; }
  size_t last_received_data_len() const { return last_received_data_len_; }
  cricket::ReceiveDataParams last_received_data_params() const {
    return last_received_data_params_;
  }

 private:
  bool has_received_data_;
  std::string last_received_data_;
  size_t last_received_data_len_;
  cricket::ReceiveDataParams last_received_data_params_;
};

class RtpDataMediaChannelTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Seed needed for each test to satisfy expectations.
    iface_.reset(new cricket::FakeNetworkInterface());
    timing_ = new FakeTiming();
    dme_.reset(CreateEngine(timing_));
    receiver_.reset(new FakeDataReceiver());
  }

  void SetNow(double now) {
    timing_->set_now(now);
  }

  cricket::RtpDataEngine* CreateEngine(FakeTiming* timing) {
    cricket::RtpDataEngine* dme = new cricket::RtpDataEngine();
    dme->SetTiming(timing);
    return dme;
  }

  cricket::RtpDataMediaChannel* CreateChannel() {
    return CreateChannel(dme_.get());
  }

  cricket::RtpDataMediaChannel* CreateChannel(cricket::RtpDataEngine* dme) {
    cricket::RtpDataMediaChannel* channel =
        static_cast<cricket::RtpDataMediaChannel*>(dme->CreateChannel(
            cricket::DCT_RTP));
    channel->SetInterface(iface_.get());
    channel->SignalDataReceived.connect(
        receiver_.get(), &FakeDataReceiver::OnDataReceived);
    return channel;
  }

  FakeDataReceiver* receiver() {
    return receiver_.get();
  }

  bool HasReceivedData() {
    return receiver_->has_received_data();
  }

  std::string GetReceivedData() {
    return receiver_->last_received_data();
  }

  size_t GetReceivedDataLen() {
    return receiver_->last_received_data_len();
  }

  cricket::ReceiveDataParams GetReceivedDataParams() {
    return receiver_->last_received_data_params();
  }

  bool HasSentData(int count) {
    return (iface_->NumRtpPackets() > count);
  }

  std::string GetSentData(int index) {
    // Assume RTP header of length 12
    const talk_base::Buffer* packet = iface_->GetRtpPacket(index);
    if (packet->length() > 12) {
      return std::string(packet->data() + 12, packet->length() - 12);
    } else {
      return "";
    }
  }

  cricket::RtpHeader GetSentDataHeader(int index) {
    const talk_base::Buffer* packet = iface_->GetRtpPacket(index);
    cricket::RtpHeader header;
    GetRtpHeader(packet->data(), packet->length(), &header);
    return header;
  }

 private:
  talk_base::scoped_ptr<cricket::RtpDataEngine> dme_;
  // Timing passed into dme_.  Owned by dme_;
  FakeTiming* timing_;
  talk_base::scoped_ptr<cricket::FakeNetworkInterface> iface_;
  talk_base::scoped_ptr<FakeDataReceiver> receiver_;
};

TEST_F(RtpDataMediaChannelTest, SetUnknownCodecs) {
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::DataCodec known_codec;
  known_codec.id = 103;
  known_codec.name = "google-data";
  cricket::DataCodec unknown_codec;
  unknown_codec.id = 104;
  unknown_codec.name = "unknown-data";

  std::vector<cricket::DataCodec> known_codecs;
  known_codecs.push_back(known_codec);

  std::vector<cricket::DataCodec> unknown_codecs;
  unknown_codecs.push_back(unknown_codec);

  std::vector<cricket::DataCodec> mixed_codecs;
  mixed_codecs.push_back(known_codec);
  mixed_codecs.push_back(unknown_codec);

  EXPECT_TRUE(dmc->SetSendCodecs(known_codecs));
  EXPECT_FALSE(dmc->SetSendCodecs(unknown_codecs));
  EXPECT_TRUE(dmc->SetSendCodecs(mixed_codecs));
  EXPECT_TRUE(dmc->SetRecvCodecs(known_codecs));
  EXPECT_FALSE(dmc->SetRecvCodecs(unknown_codecs));
  EXPECT_FALSE(dmc->SetRecvCodecs(mixed_codecs));
}

TEST_F(RtpDataMediaChannelTest, AddRemoveSendStream) {
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::StreamParams stream1;
  stream1.add_ssrc(41);
  EXPECT_TRUE(dmc->AddSendStream(stream1));
  cricket::StreamParams stream2;
  stream2.add_ssrc(42);
  EXPECT_TRUE(dmc->AddSendStream(stream2));

  EXPECT_TRUE(dmc->RemoveSendStream(41));
  EXPECT_TRUE(dmc->RemoveSendStream(42));
  EXPECT_FALSE(dmc->RemoveSendStream(43));
}

TEST_F(RtpDataMediaChannelTest, AddRemoveRecvStream) {
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::StreamParams stream1;
  stream1.add_ssrc(41);
  EXPECT_TRUE(dmc->AddRecvStream(stream1));
  cricket::StreamParams stream2;
  stream2.add_ssrc(42);
  EXPECT_TRUE(dmc->AddRecvStream(stream2));
  EXPECT_FALSE(dmc->AddRecvStream(stream2));

  EXPECT_TRUE(dmc->RemoveRecvStream(41));
  EXPECT_TRUE(dmc->RemoveRecvStream(42));
}

TEST_F(RtpDataMediaChannelTest, SendData) {
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  cricket::SendDataParams params;
  params.ssrc = 42;
  unsigned char data[] = "food";
  talk_base::Buffer payload(data, 4);
  unsigned char padded_data[] = {
    0x00, 0x00, 0x00, 0x00,
    'f', 'o', 'o', 'd',
  };
  cricket::SendDataResult result;

  // Not sending
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));
  ASSERT_TRUE(dmc->SetSend(true));

  // Unknown stream name.
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddSendStream(stream));

  // Unknown codec;
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  std::vector<cricket::DataCodec> codecs;
  codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetSendCodecs(codecs));

  // Length too large;
  std::string x10000(10000, 'x');
  EXPECT_FALSE(dmc->SendData(
      params, talk_base::Buffer(x10000.data(), x10000.length()), &result));
  EXPECT_EQ(cricket::SDR_ERROR, result);
  EXPECT_FALSE(HasSentData(0));

  // Finally works!
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  ASSERT_TRUE(HasSentData(0));
  EXPECT_EQ(sizeof(padded_data), GetSentData(0).length());
  EXPECT_EQ(0, memcmp(
      padded_data, GetSentData(0).data(), sizeof(padded_data)));
  cricket::RtpHeader header0 = GetSentDataHeader(0);
  EXPECT_NE(0, header0.seq_num);
  EXPECT_NE(0U, header0.timestamp);
  EXPECT_EQ(header0.ssrc, 42U);
  EXPECT_EQ(header0.payload_type, 103);

  // Should bump timestamp by 180000 because the clock rate is 90khz.
  SetNow(2);

  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  ASSERT_TRUE(HasSentData(1));
  EXPECT_EQ(sizeof(padded_data), GetSentData(1).length());
  EXPECT_EQ(0, memcmp(
      padded_data, GetSentData(1).data(), sizeof(padded_data)));
  cricket::RtpHeader header1 = GetSentDataHeader(1);
  EXPECT_EQ(header1.ssrc, 42U);
  EXPECT_EQ(header1.payload_type, 103);
  EXPECT_EQ(header0.seq_num + 1, header1.seq_num);
  EXPECT_EQ(header0.timestamp + 180000, header1.timestamp);
}

TEST_F(RtpDataMediaChannelTest, SendDataMultipleClocks) {
  // Timings owned by RtpDataEngines.
  FakeTiming* timing1 = new FakeTiming();
  talk_base::scoped_ptr<cricket::RtpDataEngine> dme1(CreateEngine(timing1));
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc1(
      CreateChannel(dme1.get()));
  FakeTiming* timing2 = new FakeTiming();
  talk_base::scoped_ptr<cricket::RtpDataEngine> dme2(CreateEngine(timing2));
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc2(
      CreateChannel(dme2.get()));

  ASSERT_TRUE(dmc1->SetSend(true));
  ASSERT_TRUE(dmc2->SetSend(true));

  cricket::StreamParams stream1;
  stream1.add_ssrc(41);
  ASSERT_TRUE(dmc1->AddSendStream(stream1));
  cricket::StreamParams stream2;
  stream2.add_ssrc(42);
  ASSERT_TRUE(dmc2->AddSendStream(stream2));

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  std::vector<cricket::DataCodec> codecs;
  codecs.push_back(codec);
  ASSERT_TRUE(dmc1->SetSendCodecs(codecs));
  ASSERT_TRUE(dmc2->SetSendCodecs(codecs));

  cricket::SendDataParams params1;
  params1.ssrc = 41;
  cricket::SendDataParams params2;
  params2.ssrc = 42;

  unsigned char data[] = "foo";
  talk_base::Buffer payload(data, 3);
  cricket::SendDataResult result;

  EXPECT_TRUE(dmc1->SendData(params1, payload, &result));
  EXPECT_TRUE(dmc2->SendData(params2, payload, &result));

  // Should bump timestamp by 90000 because the clock rate is 90khz.
  timing1->set_now(1);
  // Should bump timestamp by 180000 because the clock rate is 90khz.
  timing2->set_now(2);

  EXPECT_TRUE(dmc1->SendData(params1, payload, &result));
  EXPECT_TRUE(dmc2->SendData(params2, payload, &result));

  ASSERT_TRUE(HasSentData(3));
  cricket::RtpHeader header1a = GetSentDataHeader(0);
  cricket::RtpHeader header2a = GetSentDataHeader(1);
  cricket::RtpHeader header1b = GetSentDataHeader(2);
  cricket::RtpHeader header2b = GetSentDataHeader(3);

  EXPECT_EQ(header1a.seq_num + 1, header1b.seq_num);
  EXPECT_EQ(header1a.timestamp + 90000, header1b.timestamp);
  EXPECT_EQ(header2a.seq_num + 1, header2b.seq_num);
  EXPECT_EQ(header2a.timestamp + 180000, header2b.timestamp);
}

TEST_F(RtpDataMediaChannelTest, SendDataRate) {
  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  ASSERT_TRUE(dmc->SetSend(true));

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  std::vector<cricket::DataCodec> codecs;
  codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetSendCodecs(codecs));

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddSendStream(stream));

  cricket::SendDataParams params;
  params.ssrc = 42;
  unsigned char data[] = "food";
  talk_base::Buffer payload(data, 4);
  cricket::SendDataResult result;

  // With rtp overhead of 32 bytes, each one of our packets is 36
  // bytes, or 288 bits.  So, a limit of 872bps will allow 3 packets,
  // but not four.
  dmc->SetSendBandwidth(false, 872);

  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));

  SetNow(0.9);
  EXPECT_FALSE(dmc->SendData(params, payload, &result));

  SetNow(1.1);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  SetNow(1.9);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));

  SetNow(2.2);
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_TRUE(dmc->SendData(params, payload, &result));
  EXPECT_FALSE(dmc->SendData(params, payload, &result));
}

TEST_F(RtpDataMediaChannelTest, ReceiveData) {
  // PT= 103, SN=2, TS=3, SSRC = 4, data = "abcde"
  unsigned char data[] = {
    0x80, 0x67, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2A,
    0x00, 0x00, 0x00, 0x00,
    'a', 'b', 'c', 'd', 'e'
  };
  talk_base::Buffer packet(data, sizeof(data));

  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  // SetReceived not called.
  dmc->OnPacketReceived(&packet);
  EXPECT_FALSE(HasReceivedData());

  dmc->SetReceive(true);

  // Unknown payload id
  dmc->OnPacketReceived(&packet);
  EXPECT_FALSE(HasReceivedData());

  cricket::DataCodec codec;
  codec.id = 103;
  codec.name = cricket::kGoogleRtpDataCodecName;
  std::vector<cricket::DataCodec> codecs;
  codecs.push_back(codec);
  ASSERT_TRUE(dmc->SetRecvCodecs(codecs));

  // Unknown stream
  dmc->OnPacketReceived(&packet);
  EXPECT_FALSE(HasReceivedData());

  cricket::StreamParams stream;
  stream.add_ssrc(42);
  ASSERT_TRUE(dmc->AddRecvStream(stream));

  // Finally works!
  dmc->OnPacketReceived(&packet);
  EXPECT_TRUE(HasReceivedData());
  EXPECT_EQ("abcde", GetReceivedData());
  EXPECT_EQ(5U, GetReceivedDataLen());
}

TEST_F(RtpDataMediaChannelTest, InvalidRtpPackets) {
  unsigned char data[] = {
    0x80, 0x65, 0x00, 0x02
  };
  talk_base::Buffer packet(data, sizeof(data));

  talk_base::scoped_ptr<cricket::RtpDataMediaChannel> dmc(CreateChannel());

  // Too short
  dmc->OnPacketReceived(&packet);
  EXPECT_FALSE(HasReceivedData());
}
