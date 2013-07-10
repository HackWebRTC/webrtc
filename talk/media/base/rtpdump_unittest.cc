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

#include "talk/base/bytebuffer.h"
#include "talk/base/gunit.h"
#include "talk/base/thread.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/base/testutils.h"

namespace cricket {

static const uint32 kTestSsrc = 1;

// Test that we read the correct header fields from the RTP/RTCP packet.
TEST(RtpDumpTest, ReadRtpDumpPacket) {
  talk_base::ByteBuffer rtp_buf;
  RtpTestUtility::kTestRawRtpPackets[0].WriteToByteBuffer(kTestSsrc, &rtp_buf);
  RtpDumpPacket rtp_packet(rtp_buf.Data(), rtp_buf.Length(), 0, false);

  int type;
  int seq_num;
  uint32 ts;
  uint32 ssrc;
  EXPECT_FALSE(rtp_packet.is_rtcp());
  EXPECT_TRUE(rtp_packet.IsValidRtpPacket());
  EXPECT_FALSE(rtp_packet.IsValidRtcpPacket());
  EXPECT_TRUE(rtp_packet.GetRtpPayloadType(&type));
  EXPECT_EQ(0, type);
  EXPECT_TRUE(rtp_packet.GetRtpSeqNum(&seq_num));
  EXPECT_EQ(0, seq_num);
  EXPECT_TRUE(rtp_packet.GetRtpTimestamp(&ts));
  EXPECT_EQ(0U, ts);
  EXPECT_TRUE(rtp_packet.GetRtpSsrc(&ssrc));
  EXPECT_EQ(kTestSsrc, ssrc);
  EXPECT_FALSE(rtp_packet.GetRtcpType(&type));

  talk_base::ByteBuffer rtcp_buf;
  RtpTestUtility::kTestRawRtcpPackets[0].WriteToByteBuffer(&rtcp_buf);
  RtpDumpPacket rtcp_packet(rtcp_buf.Data(), rtcp_buf.Length(), 0, true);

  EXPECT_TRUE(rtcp_packet.is_rtcp());
  EXPECT_FALSE(rtcp_packet.IsValidRtpPacket());
  EXPECT_TRUE(rtcp_packet.IsValidRtcpPacket());
  EXPECT_TRUE(rtcp_packet.GetRtcpType(&type));
  EXPECT_EQ(0, type);
}

// Test that we read only the RTP dump file.
TEST(RtpDumpTest, ReadRtpDumpFile) {
  RtpDumpPacket packet;
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  talk_base::scoped_ptr<RtpDumpReader> reader;

  // Write a RTP packet to the stream, which is a valid RTP dump. Next, we will
  // change the first line to make the RTP dump valid or invalid.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, false, kTestSsrc, &writer));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(talk_base::SR_SUCCESS, reader->ReadPacket(&packet));

  // The first line is correct.
  stream.Rewind();
  const char new_line[] = "#!rtpplay1.0 1.1.1.1/1\n";
  EXPECT_EQ(talk_base::SR_SUCCESS,
            stream.WriteAll(new_line, strlen(new_line), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(talk_base::SR_SUCCESS, reader->ReadPacket(&packet));

  // The first line is not correct: not started with #!rtpplay1.0.
  stream.Rewind();
  const char new_line2[] = "#!rtpplaz1.0 0.0.0.0/0\n";
  EXPECT_EQ(talk_base::SR_SUCCESS,
            stream.WriteAll(new_line2, strlen(new_line2), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(talk_base::SR_ERROR, reader->ReadPacket(&packet));

  // The first line is not correct: no port.
  stream.Rewind();
  const char new_line3[] = "#!rtpplay1.0 0.0.0.0//\n";
  EXPECT_EQ(talk_base::SR_SUCCESS,
            stream.WriteAll(new_line3, strlen(new_line3), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(talk_base::SR_ERROR, reader->ReadPacket(&packet));
}

// Test that we read the same RTP packets that rtp dump writes.
TEST(RtpDumpTest, WriteReadSameRtp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));

  // Check stream has only RtpTestUtility::GetTestPacketCount() packets.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
    uint32 ssrc;
    EXPECT_TRUE(GetRtpSsrc(&packet.data[0], packet.data.size(), &ssrc));
    EXPECT_EQ(kTestSsrc, ssrc);
  }
  // No more packets to read.
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));

  // Rewind the stream and read again with a specified ssrc.
  stream.Rewind();
  RtpDumpReader reader_w_ssrc(&stream);
  const uint32 send_ssrc = kTestSsrc + 1;
  reader_w_ssrc.SetSsrc(send_ssrc);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(talk_base::SR_SUCCESS, reader_w_ssrc.ReadPacket(&packet));
    EXPECT_FALSE(packet.is_rtcp());
    EXPECT_EQ(packet.original_data_len, packet.data.size());
    uint32 ssrc;
    EXPECT_TRUE(GetRtpSsrc(&packet.data[0], packet.data.size(), &ssrc));
    EXPECT_EQ(send_ssrc, ssrc);
  }
  // No more packets to read.
  EXPECT_EQ(talk_base::SR_EOS, reader_w_ssrc.ReadPacket(&packet));
}

// Test that we read the same RTCP packets that rtp dump writes.
TEST(RtpDumpTest, WriteReadSameRtcp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));

  // Check stream has only RtpTestUtility::GetTestPacketCount() packets.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  reader.SetSsrc(kTestSsrc + 1);  // Does not affect RTCP packet.
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_TRUE(packet.is_rtcp());
    EXPECT_EQ(0U, packet.original_data_len);
  }
  // No more packets to read.
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));
}

// Test dumping only RTP packet headers.
TEST(RtpDumpTest, WriteReadRtpHeadersOnly) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  writer.set_packet_filter(PF_RTPHEADER);

  // Write some RTP and RTCP packets. RTP packets should only have headers;
  // RTCP packets should be eaten.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  stream.Rewind();

  // Check that only RTP packet headers are present.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_FALSE(packet.is_rtcp());
    size_t len = 0;
    packet.GetRtpHeaderLen(&len);
    EXPECT_EQ(len, packet.data.size());
    EXPECT_GT(packet.original_data_len, packet.data.size());
  }
  // No more packets to read.
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));
}

// Test dumping only RTCP packets.
TEST(RtpDumpTest, WriteReadRtcpOnly) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  writer.set_packet_filter(PF_RTCPPACKET);

  // Write some RTP and RTCP packets. RTP packets should be eaten.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  stream.Rewind();

  // Check that only RTCP packets are present.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_TRUE(packet.is_rtcp());
    EXPECT_EQ(0U, packet.original_data_len);
  }
  // No more packets to read.
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));
}

// Test that RtpDumpLoopReader reads RTP packets continously and the elapsed
// time, the sequence number, and timestamp are maintained properly.
TEST(RtpDumpTest, LoopReadRtp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      3 * RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));
}

// Test that RtpDumpLoopReader reads RTCP packets continously and the elapsed
// time is maintained properly.
TEST(RtpDumpTest, LoopReadRtcp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      3 * RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));
}

// Test that RtpDumpLoopReader reads continously from stream with a single RTP
// packets.
TEST(RtpDumpTest, LoopReadSingleRtp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, false, kTestSsrc, &writer));

  // The regular reader can read only one packet.
  RtpDumpPacket packet;
  stream.Rewind();
  RtpDumpReader reader(&stream);
  EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));

  // The loop reader reads three packets from the input stream.
  stream.Rewind();
  RtpDumpLoopReader loop_reader(&stream);
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
}

// Test that RtpDumpLoopReader reads continously from stream with a single RTCP
// packets.
TEST(RtpDumpTest, LoopReadSingleRtcp) {
  talk_base::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, true, kTestSsrc, &writer));

  // The regular reader can read only one packet.
  RtpDumpPacket packet;
  stream.Rewind();
  RtpDumpReader reader(&stream);
  EXPECT_EQ(talk_base::SR_SUCCESS, reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_EOS, reader.ReadPacket(&packet));

  // The loop reader reads three packets from the input stream.
  stream.Rewind();
  RtpDumpLoopReader loop_reader(&stream);
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(talk_base::SR_SUCCESS, loop_reader.ReadPacket(&packet));
}

}  // namespace cricket
