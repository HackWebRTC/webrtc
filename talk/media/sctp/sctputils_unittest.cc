/*
 * libjingle
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

#include "talk/app/webrtc/datachannelinterface.h"
#include "talk/base/bytebuffer.h"
#include "talk/base/gunit.h"
#include "talk/media/sctp/sctputils.h"

class SctpUtilsTest : public testing::Test {
 public:
  void VerifyOpenMessageFormat(const talk_base::Buffer& packet,
                               const std::string& label,
                               const webrtc::DataChannelInit& config) {
    uint8 message_type;
    uint8 channel_type;
    uint32 reliability;
    uint16 priority;
    uint16 label_length;
    uint16 protocol_length;

    talk_base::ByteBuffer buffer(packet.data(), packet.length());
    ASSERT_TRUE(buffer.ReadUInt8(&message_type));
    EXPECT_EQ(0x03, message_type);

    ASSERT_TRUE(buffer.ReadUInt8(&channel_type));
    if (config.ordered) {
      EXPECT_EQ(config.maxRetransmits > -1 ?
                    0x01 : (config.maxRetransmitTime > -1 ? 0x02 : 0),
                channel_type);
    } else {
      EXPECT_EQ(config.maxRetransmits > -1 ?
                    0x81 : (config.maxRetransmitTime > -1 ? 0x82 : 0x80),
                channel_type);
    }

    ASSERT_TRUE(buffer.ReadUInt16(&priority));

    ASSERT_TRUE(buffer.ReadUInt32(&reliability));
    if (config.maxRetransmits > -1 || config.maxRetransmitTime > -1) {
      EXPECT_EQ(config.maxRetransmits > -1 ?
                    config.maxRetransmits : config.maxRetransmitTime,
                static_cast<int>(reliability));
    }

    ASSERT_TRUE(buffer.ReadUInt16(&label_length));
    ASSERT_TRUE(buffer.ReadUInt16(&protocol_length));
    EXPECT_EQ(label.size(), label_length);
    EXPECT_EQ(config.protocol.size(), protocol_length);

    std::string label_output;
    ASSERT_TRUE(buffer.ReadString(&label_output, label_length));
    EXPECT_EQ(label, label_output);
    std::string protocol_output;
    ASSERT_TRUE(buffer.ReadString(&protocol_output, protocol_length));
    EXPECT_EQ(config.protocol, protocol_output);
  }
};

TEST_F(SctpUtilsTest, WriteParseMessageWithOrderedReliable) {
  std::string input_label = "abc";
  webrtc::DataChannelInit config;
  config.protocol = "y";

  talk_base::Buffer packet;
  ASSERT_TRUE(
      cricket::WriteDataChannelOpenMessage(input_label, config, &packet));

  VerifyOpenMessageFormat(packet, input_label, config);

  std::string output_label;
  webrtc::DataChannelInit output_config;
  ASSERT_TRUE(cricket::ParseDataChannelOpenMessage(
      packet, &output_label, &output_config));

  EXPECT_EQ(input_label, output_label);
  EXPECT_EQ(config.protocol, output_config.protocol);
  EXPECT_EQ(config.ordered, output_config.ordered);
  EXPECT_EQ(config.maxRetransmitTime, output_config.maxRetransmitTime);
  EXPECT_EQ(config.maxRetransmits, output_config.maxRetransmits);
}

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithMaxRetransmitTime) {
  std::string input_label = "abc";
  webrtc::DataChannelInit config;
  config.ordered = false;
  config.maxRetransmitTime = 10;
  config.protocol = "y";

  talk_base::Buffer packet;
  ASSERT_TRUE(
      cricket::WriteDataChannelOpenMessage(input_label, config, &packet));

  VerifyOpenMessageFormat(packet, input_label, config);

  std::string output_label;
  webrtc::DataChannelInit output_config;
  ASSERT_TRUE(cricket::ParseDataChannelOpenMessage(
      packet, &output_label, &output_config));

  EXPECT_EQ(input_label, output_label);
  EXPECT_EQ(config.protocol, output_config.protocol);
  EXPECT_EQ(config.ordered, output_config.ordered);
  EXPECT_EQ(config.maxRetransmitTime, output_config.maxRetransmitTime);
  EXPECT_EQ(-1, output_config.maxRetransmits);
}

TEST_F(SctpUtilsTest, WriteParseOpenMessageWithMaxRetransmits) {
  std::string input_label = "abc";
  webrtc::DataChannelInit config;
  config.maxRetransmits = 10;
  config.protocol = "y";

  talk_base::Buffer packet;
  ASSERT_TRUE(
      cricket::WriteDataChannelOpenMessage(input_label, config, &packet));

  VerifyOpenMessageFormat(packet, input_label, config);

  std::string output_label;
  webrtc::DataChannelInit output_config;
  ASSERT_TRUE(cricket::ParseDataChannelOpenMessage(
      packet, &output_label, &output_config));

  EXPECT_EQ(input_label, output_label);
  EXPECT_EQ(config.protocol, output_config.protocol);
  EXPECT_EQ(config.ordered, output_config.ordered);
  EXPECT_EQ(config.maxRetransmits, output_config.maxRetransmits);
  EXPECT_EQ(-1, output_config.maxRetransmitTime);
}
