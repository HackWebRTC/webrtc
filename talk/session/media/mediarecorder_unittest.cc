// libjingle
// Copyright 2010 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>

#include "talk/base/bytebuffer.h"
#include "talk/base/fileutils.h"
#include "talk/base/gunit.h"
#include "talk/base/pathutils.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/testutils.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/mediarecorder.h"

namespace cricket {

talk_base::StreamInterface* Open(const std::string& path) {
  return talk_base::Filesystem::OpenFile(
      talk_base::Pathname(path), "wb");
}

/////////////////////////////////////////////////////////////////////////
// Test RtpDumpSink
/////////////////////////////////////////////////////////////////////////
class RtpDumpSinkTest : public testing::Test {
 public:
  virtual void SetUp() {
    EXPECT_TRUE(talk_base::Filesystem::GetTemporaryFolder(path_, true, NULL));
    path_.SetFilename("sink-test.rtpdump");
    sink_.reset(new RtpDumpSink(Open(path_.pathname())));

    for (int i = 0; i < ARRAY_SIZE(rtp_buf_); ++i) {
      RtpTestUtility::kTestRawRtpPackets[i].WriteToByteBuffer(
          RtpTestUtility::kDefaultSsrc, &rtp_buf_[i]);
    }
  }

  virtual void TearDown() {
    stream_.reset();
    EXPECT_TRUE(talk_base::Filesystem::DeleteFile(path_));
  }

 protected:
  void OnRtpPacket(const RawRtpPacket& raw) {
    talk_base::ByteBuffer buf;
    raw.WriteToByteBuffer(RtpTestUtility::kDefaultSsrc, &buf);
    sink_->OnPacket(buf.Data(), buf.Length(), false);
  }

  talk_base::StreamResult ReadPacket(RtpDumpPacket* packet) {
    if (!stream_.get()) {
      sink_.reset();  // This will close the file. So we can read it.
      stream_.reset(talk_base::Filesystem::OpenFile(path_, "rb"));
      reader_.reset(new RtpDumpReader(stream_.get()));
    }
    return reader_->ReadPacket(packet);
  }

  talk_base::Pathname path_;
  talk_base::scoped_ptr<RtpDumpSink> sink_;
  talk_base::ByteBuffer rtp_buf_[3];
  talk_base::scoped_ptr<talk_base::StreamInterface> stream_;
  talk_base::scoped_ptr<RtpDumpReader> reader_;
};

TEST_F(RtpDumpSinkTest, TestRtpDumpSink) {
  // By default, the sink is disabled. The 1st packet is not written.
  EXPECT_FALSE(sink_->IsEnabled());
  sink_->set_packet_filter(PF_ALL);
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[0]);

  // Enable the sink. The 2nd packet is written.
  EXPECT_TRUE(sink_->Enable(true));
  EXPECT_TRUE(sink_->IsEnabled());
  EXPECT_TRUE(talk_base::Filesystem::IsFile(path_.pathname()));
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[1]);

  // Disable the sink. The 3rd packet is not written.
  EXPECT_TRUE(sink_->Enable(false));
  EXPECT_FALSE(sink_->IsEnabled());
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[2]);

  // Read the recorded file and verify it contains only the 2nd packet.
  RtpDumpPacket packet;
  EXPECT_EQ(talk_base::SR_SUCCESS, ReadPacket(&packet));
  EXPECT_TRUE(RtpTestUtility::VerifyPacket(
      &packet, &RtpTestUtility::kTestRawRtpPackets[1], false));
  EXPECT_EQ(talk_base::SR_EOS, ReadPacket(&packet));
}

TEST_F(RtpDumpSinkTest, TestRtpDumpSinkMaxSize) {
  EXPECT_TRUE(sink_->Enable(true));
  sink_->set_packet_filter(PF_ALL);
  sink_->SetMaxSize(strlen(RtpDumpFileHeader::kFirstLine) +
                    RtpDumpFileHeader::kHeaderLength +
                    RtpDumpPacket::kHeaderLength +
                    RtpTestUtility::kTestRawRtpPackets[0].size());
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[0]);

  // Exceed the limit size: the 2nd and 3rd packets are not written.
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[1]);
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[2]);

  // Read the recorded file and verify that it contains only the first packet.
  RtpDumpPacket packet;
  EXPECT_EQ(talk_base::SR_SUCCESS, ReadPacket(&packet));
  EXPECT_TRUE(RtpTestUtility::VerifyPacket(
      &packet, &RtpTestUtility::kTestRawRtpPackets[0], false));
  EXPECT_EQ(talk_base::SR_EOS, ReadPacket(&packet));
}

TEST_F(RtpDumpSinkTest, TestRtpDumpSinkFilter) {
  // The default filter is PF_NONE.
  EXPECT_EQ(PF_NONE, sink_->packet_filter());

  // Set to PF_RTPHEADER before enable.
  sink_->set_packet_filter(PF_RTPHEADER);
  EXPECT_EQ(PF_RTPHEADER, sink_->packet_filter());
  EXPECT_TRUE(sink_->Enable(true));
  // We dump only the header of the first packet.
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[0]);

  // Set the filter to PF_RTPPACKET. We dump all the second packet.
  sink_->set_packet_filter(PF_RTPPACKET);
  EXPECT_EQ(PF_RTPPACKET, sink_->packet_filter());
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[1]);

  // Set the filter to PF_NONE. We do not dump the third packet.
  sink_->set_packet_filter(PF_NONE);
  EXPECT_EQ(PF_NONE, sink_->packet_filter());
  OnRtpPacket(RtpTestUtility::kTestRawRtpPackets[2]);

  // Read the recorded file and verify the header of the first packet and
  // the whole packet for the second packet.
  RtpDumpPacket packet;
  EXPECT_EQ(talk_base::SR_SUCCESS, ReadPacket(&packet));
  EXPECT_TRUE(RtpTestUtility::VerifyPacket(
      &packet, &RtpTestUtility::kTestRawRtpPackets[0], true));
  EXPECT_EQ(talk_base::SR_SUCCESS, ReadPacket(&packet));
  EXPECT_TRUE(RtpTestUtility::VerifyPacket(
      &packet, &RtpTestUtility::kTestRawRtpPackets[1], false));
  EXPECT_EQ(talk_base::SR_EOS, ReadPacket(&packet));
}

/////////////////////////////////////////////////////////////////////////
// Test MediaRecorder
/////////////////////////////////////////////////////////////////////////
void TestMediaRecorder(BaseChannel* channel,
                       FakeVideoMediaChannel* video_media_channel,
                       int filter) {
  // Create media recorder.
  talk_base::scoped_ptr<MediaRecorder> recorder(new MediaRecorder);
  // Fail to EnableChannel before AddChannel.
  EXPECT_FALSE(recorder->EnableChannel(channel, true, true, SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasSendSinks(SINK_POST_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_POST_CRYPTO));

  // Add the channel to the recorder.
  talk_base::Pathname path;
  EXPECT_TRUE(talk_base::Filesystem::GetTemporaryFolder(path, true, NULL));
  path.SetFilename("send.rtpdump");
  std::string send_file = path.pathname();
  path.SetFilename("recv.rtpdump");
  std::string recv_file = path.pathname();
  if (video_media_channel) {
    EXPECT_TRUE(recorder->AddChannel(static_cast<VideoChannel*>(channel),
                                     Open(send_file), Open(recv_file), filter));
  } else {
    EXPECT_TRUE(recorder->AddChannel(static_cast<VoiceChannel*>(channel),
                                     Open(send_file), Open(recv_file), filter));
  }

  // Enable recording only the sent media.
  EXPECT_TRUE(recorder->EnableChannel(channel, true, false, SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_POST_CRYPTO));
  EXPECT_FALSE(channel->HasSendSinks(SINK_POST_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_POST_CRYPTO));
  if (video_media_channel) {
    EXPECT_TRUE_WAIT(video_media_channel->sent_intra_frame(), 100);
  }

  // Enable recording only the received meida.
  EXPECT_TRUE(recorder->EnableChannel(channel, false, true, SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_PRE_CRYPTO));
  if (video_media_channel) {
    EXPECT_TRUE(video_media_channel->requested_intra_frame());
  }

  // Enable recording both the sent and the received video.
  EXPECT_TRUE(recorder->EnableChannel(channel, true, true, SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_PRE_CRYPTO));

  // Enable recording only headers.
  if (video_media_channel) {
    video_media_channel->set_sent_intra_frame(false);
    video_media_channel->set_requested_intra_frame(false);
  }
  EXPECT_TRUE(recorder->EnableChannel(channel, true, true, SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_PRE_CRYPTO));
  if (video_media_channel) {
    if ((filter & PF_RTPPACKET) == PF_RTPPACKET) {
      // If record the whole RTP packet, trigers FIR.
      EXPECT_TRUE(video_media_channel->requested_intra_frame());
      EXPECT_TRUE(video_media_channel->sent_intra_frame());
    } else {
      // If record only the RTP header, does not triger FIR.
      EXPECT_FALSE(video_media_channel->requested_intra_frame());
      EXPECT_FALSE(video_media_channel->sent_intra_frame());
    }
  }

  // Remove the voice channel from the recorder.
  recorder->RemoveChannel(channel, SINK_PRE_CRYPTO);
  EXPECT_FALSE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_PRE_CRYPTO));

  // Delete all files.
  recorder.reset();
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(send_file));
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(recv_file));
}

// Fisrt start recording header and then start recording media. Verify that
// differnt files are created for header and media.
void TestRecordHeaderAndMedia(BaseChannel* channel,
                              FakeVideoMediaChannel* video_media_channel) {
  // Create RTP header recorder.
  talk_base::scoped_ptr<MediaRecorder> header_recorder(new MediaRecorder);

  talk_base::Pathname path;
  EXPECT_TRUE(talk_base::Filesystem::GetTemporaryFolder(path, true, NULL));
  path.SetFilename("send-header.rtpdump");
  std::string send_header_file = path.pathname();
  path.SetFilename("recv-header.rtpdump");
  std::string recv_header_file = path.pathname();
  if (video_media_channel) {
    EXPECT_TRUE(header_recorder->AddChannel(
        static_cast<VideoChannel*>(channel),
        Open(send_header_file), Open(recv_header_file), PF_RTPHEADER));
  } else {
    EXPECT_TRUE(header_recorder->AddChannel(
        static_cast<VoiceChannel*>(channel),
        Open(send_header_file), Open(recv_header_file), PF_RTPHEADER));
  }

  // Enable recording both sent and received.
  EXPECT_TRUE(
      header_recorder->EnableChannel(channel, true, true, SINK_POST_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_POST_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_POST_CRYPTO));
  EXPECT_FALSE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_FALSE(channel->HasRecvSinks(SINK_PRE_CRYPTO));
  if (video_media_channel) {
    EXPECT_FALSE(video_media_channel->sent_intra_frame());
    EXPECT_FALSE(video_media_channel->requested_intra_frame());
  }

  // Verify that header files are created.
  EXPECT_TRUE(talk_base::Filesystem::IsFile(send_header_file));
  EXPECT_TRUE(talk_base::Filesystem::IsFile(recv_header_file));

  // Create RTP header recorder.
  talk_base::scoped_ptr<MediaRecorder> recorder(new MediaRecorder);
  path.SetFilename("send.rtpdump");
  std::string send_file = path.pathname();
  path.SetFilename("recv.rtpdump");
  std::string recv_file = path.pathname();
  if (video_media_channel) {
    EXPECT_TRUE(recorder->AddChannel(
        static_cast<VideoChannel*>(channel),
        Open(send_file), Open(recv_file), PF_RTPPACKET));
  } else {
    EXPECT_TRUE(recorder->AddChannel(
        static_cast<VoiceChannel*>(channel),
        Open(send_file), Open(recv_file), PF_RTPPACKET));
  }

  // Enable recording both sent and received.
  EXPECT_TRUE(recorder->EnableChannel(channel, true, true, SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_POST_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_POST_CRYPTO));
  EXPECT_TRUE(channel->HasSendSinks(SINK_PRE_CRYPTO));
  EXPECT_TRUE(channel->HasRecvSinks(SINK_PRE_CRYPTO));
  if (video_media_channel) {
    EXPECT_TRUE_WAIT(video_media_channel->sent_intra_frame(), 100);
    EXPECT_TRUE(video_media_channel->requested_intra_frame());
  }

  // Verify that media files are created.
  EXPECT_TRUE(talk_base::Filesystem::IsFile(send_file));
  EXPECT_TRUE(talk_base::Filesystem::IsFile(recv_file));

  // Delete all files.
  header_recorder.reset();
  recorder.reset();
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(send_header_file));
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(recv_header_file));
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(send_file));
  EXPECT_TRUE(talk_base::Filesystem::DeleteFile(recv_file));
}

TEST(MediaRecorderTest, TestMediaRecorderVoiceChannel) {
  // Create the voice channel.
  FakeSession session(true);
  FakeMediaEngine media_engine;
  VoiceChannel channel(talk_base::Thread::Current(), &media_engine,
                       new FakeVoiceMediaChannel(NULL), &session, "", false);
  EXPECT_TRUE(channel.Init());
  TestMediaRecorder(&channel, NULL, PF_RTPPACKET);
  TestMediaRecorder(&channel, NULL, PF_RTPHEADER);
  TestRecordHeaderAndMedia(&channel, NULL);
}

TEST(MediaRecorderTest, TestMediaRecorderVideoChannel) {
  // Create the video channel.
  FakeSession session(true);
  FakeMediaEngine media_engine;
  FakeVideoMediaChannel* media_channel = new FakeVideoMediaChannel(NULL);
  VideoChannel channel(talk_base::Thread::Current(), &media_engine,
                       media_channel, &session, "", false, NULL);
  EXPECT_TRUE(channel.Init());
  TestMediaRecorder(&channel, media_channel, PF_RTPPACKET);
  TestMediaRecorder(&channel, media_channel, PF_RTPHEADER);
  TestRecordHeaderAndMedia(&channel, media_channel);
}

}  // namespace cricket
