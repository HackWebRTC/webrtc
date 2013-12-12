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

#include <set>

#include "talk/base/buffer.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/pathutils.h"
#include "talk/base/stream.h"
#include "talk/media/base/filemediaengine.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/testutils.h"

namespace cricket {

static const int kWaitTimeMs = 100;
static const std::string kFakeFileName = "foobar";

//////////////////////////////////////////////////////////////////////////////
// Media channel sends RTP packets via NetworkInterface. Rather than sending
// packets to the network, FileNetworkInterface writes packets to a stream and
// feeds packets back to the channel via OnPacketReceived.
//////////////////////////////////////////////////////////////////////////////
class FileNetworkInterface : public MediaChannel::NetworkInterface {
 public:
  FileNetworkInterface(talk_base::StreamInterface* output, MediaChannel* ch)
      : media_channel_(ch),
        num_sent_packets_(0) {
    if (output) {
      dump_writer_.reset(new RtpDumpWriter(output));
    }
  }

  // Implement pure virtual methods of NetworkInterface.
  virtual bool SendPacket(talk_base::Buffer* packet,
                          talk_base::DiffServCodePoint dscp) {
    if (!packet) return false;

    if (media_channel_) {
      media_channel_->OnPacketReceived(packet, talk_base::PacketTime());
    }
    if (dump_writer_.get() &&
        talk_base::SR_SUCCESS != dump_writer_->WriteRtpPacket(
            packet->data(), packet->length())) {
      return false;
    }

    ++num_sent_packets_;
    return true;
  }

  virtual bool SendRtcp(talk_base::Buffer* packet,
                        talk_base::DiffServCodePoint dscp) { return false; }
  virtual int SetOption(MediaChannel::NetworkInterface::SocketType type,
      talk_base::Socket::Option opt, int option) {
    return 0;
  }
  virtual void SetDefaultDSCPCode(talk_base::DiffServCodePoint dscp) {}

  size_t num_sent_packets() const { return num_sent_packets_; }

 private:
  MediaChannel* media_channel_;
  talk_base::scoped_ptr<RtpDumpWriter> dump_writer_;
  size_t num_sent_packets_;

  DISALLOW_COPY_AND_ASSIGN(FileNetworkInterface);
};

class FileMediaEngineTest : public testing::Test {
 public:
  virtual void SetUp() {
    setup_ok_ = true;
    setup_ok_ &= GetTempFilename(&voice_input_filename_);
    setup_ok_ &= GetTempFilename(&voice_output_filename_);
    setup_ok_ &= GetTempFilename(&video_input_filename_);
    setup_ok_ &= GetTempFilename(&video_output_filename_);
  }
  virtual void TearDown() {
    // Force to close the dump files, if opened.
    voice_channel_.reset();
    video_channel_.reset();

    DeleteTempFile(voice_input_filename_);
    DeleteTempFile(voice_output_filename_);
    DeleteTempFile(video_input_filename_);
    DeleteTempFile(video_output_filename_);
  }

 protected:
  bool CreateEngineAndChannels(const std::string& voice_in,
                               const std::string& voice_out,
                               const std::string& video_in,
                               const std::string& video_out,
                               size_t ssrc_count) {
    // Force to close the dump files, if opened.
    voice_channel_.reset();
    video_channel_.reset();

    bool ret = setup_ok_;
    if (!voice_in.empty()) {
      ret &= WriteTestPacketsToFile(voice_in, ssrc_count);
    }
    if (!video_in.empty()) {
      ret &= WriteTestPacketsToFile(video_in, ssrc_count);
    }

    engine_.reset(new FileMediaEngine);
    engine_->set_voice_input_filename(voice_in);
    engine_->set_voice_output_filename(voice_out);
    engine_->set_video_input_filename(video_in);
    engine_->set_video_output_filename(video_out);
    engine_->set_rtp_sender_thread(talk_base::Thread::Current());

    voice_channel_.reset(engine_->CreateChannel());
    video_channel_.reset(engine_->CreateVideoChannel(NULL));

    return ret;
  }

  bool GetTempFilename(std::string* filename) {
    talk_base::Pathname temp_path;
    if (!talk_base::Filesystem::GetTemporaryFolder(temp_path, true, NULL)) {
      return false;
    }
    temp_path.SetPathname(
        talk_base::Filesystem::TempFilename(temp_path, "fme-test-"));

    if (filename) {
      *filename = temp_path.pathname();
    }
    return true;
  }

  bool WriteTestPacketsToFile(const std::string& filename, size_t ssrc_count) {
    talk_base::scoped_ptr<talk_base::StreamInterface> stream(
        talk_base::Filesystem::OpenFile(talk_base::Pathname(filename), "wb"));
    bool ret = (NULL != stream.get());
    RtpDumpWriter writer(stream.get());

    for (size_t i = 0; i < ssrc_count; ++i) {
      ret &= RtpTestUtility::WriteTestPackets(
          RtpTestUtility::GetTestPacketCount(), false,
          static_cast<uint32>(RtpTestUtility::kDefaultSsrc + i),
          &writer);
    }
    return ret;
  }

  void DeleteTempFile(std::string filename) {
    talk_base::Pathname pathname(filename);
    if (talk_base::Filesystem::IsFile(talk_base::Pathname(pathname))) {
      talk_base::Filesystem::DeleteFile(pathname);
    }
  }

  bool GetSsrcAndPacketCounts(talk_base::StreamInterface* stream,
                              size_t* ssrc_count, size_t* packet_count) {
    talk_base::scoped_ptr<RtpDumpReader> reader(new RtpDumpReader(stream));
    size_t count = 0;
    RtpDumpPacket packet;
    std::set<uint32> ssrcs;
    while (talk_base::SR_SUCCESS == reader->ReadPacket(&packet)) {
      count++;
      uint32 ssrc;
      if (!packet.GetRtpSsrc(&ssrc)) {
        return false;
      }
      ssrcs.insert(ssrc);
    }
    if (ssrc_count) {
      *ssrc_count = ssrcs.size();
    }
    if (packet_count) {
      *packet_count = count;
    }
    return true;
  }

  static const uint32 kWaitTimeout = 3000;
  bool setup_ok_;
  std::string voice_input_filename_;
  std::string voice_output_filename_;
  std::string video_input_filename_;
  std::string video_output_filename_;
  talk_base::scoped_ptr<FileMediaEngine> engine_;
  talk_base::scoped_ptr<VoiceMediaChannel> voice_channel_;
  talk_base::scoped_ptr<VideoMediaChannel> video_channel_;
};

TEST_F(FileMediaEngineTest, TestDefaultImplementation) {
  EXPECT_TRUE(CreateEngineAndChannels("", "", "", "", 1));
  EXPECT_TRUE(engine_->Init(talk_base::Thread::Current()));
  EXPECT_EQ(0, engine_->GetCapabilities());
  EXPECT_TRUE(NULL == voice_channel_.get());
  EXPECT_TRUE(NULL == video_channel_.get());
  EXPECT_TRUE(NULL == engine_->CreateSoundclip());
  cricket::AudioOptions audio_options;
  EXPECT_TRUE(engine_->SetAudioOptions(audio_options));
  cricket::VideoOptions video_options;
  EXPECT_TRUE(engine_->SetVideoOptions(video_options));
  VideoEncoderConfig video_encoder_config;
  EXPECT_TRUE(engine_->SetDefaultVideoEncoderConfig(video_encoder_config));
  EXPECT_TRUE(engine_->SetSoundDevices(NULL, NULL));
  EXPECT_TRUE(engine_->SetVideoCaptureDevice(NULL));
  EXPECT_TRUE(engine_->SetOutputVolume(0));
  EXPECT_EQ(0, engine_->GetInputLevel());
  EXPECT_TRUE(engine_->SetLocalMonitor(true));
  EXPECT_TRUE(engine_->SetLocalRenderer(NULL));
  EXPECT_TRUE(engine_->SetVideoCapture(true));
  EXPECT_EQ(0U, engine_->audio_codecs().size());
  EXPECT_EQ(0U, engine_->video_codecs().size());
  AudioCodec voice_codec;
  EXPECT_TRUE(engine_->FindAudioCodec(voice_codec));
  VideoCodec video_codec;
  EXPECT_TRUE(engine_->FindVideoCodec(video_codec));
  engine_->Terminate();
}

// Test that when file path is not pointing to a valid stream file, the channel
// creation function should fail and return NULL.
TEST_F(FileMediaEngineTest, TestBadFilePath) {
  engine_.reset(new FileMediaEngine);
  engine_->set_voice_input_filename(kFakeFileName);
  engine_->set_video_input_filename(kFakeFileName);
  EXPECT_TRUE(engine_->CreateChannel() == NULL);
  EXPECT_TRUE(engine_->CreateVideoChannel(NULL) == NULL);
}

TEST_F(FileMediaEngineTest, TestCodecs) {
  EXPECT_TRUE(CreateEngineAndChannels("", "", "", "", 1));
  std::vector<AudioCodec> voice_codecs = engine_->audio_codecs();
  std::vector<VideoCodec> video_codecs = engine_->video_codecs();
  EXPECT_EQ(0U, voice_codecs.size());
  EXPECT_EQ(0U, video_codecs.size());

  AudioCodec voice_codec(103, "ISAC", 16000, 0, 1, 0);
  voice_codecs.push_back(voice_codec);
  engine_->set_voice_codecs(voice_codecs);
  voice_codecs = engine_->audio_codecs();
  ASSERT_EQ(1U, voice_codecs.size());
  EXPECT_EQ(voice_codec, voice_codecs[0]);

  VideoCodec video_codec(96, "H264-SVC", 320, 240, 30, 0);
  video_codecs.push_back(video_codec);
  engine_->set_video_codecs(video_codecs);
  video_codecs = engine_->video_codecs();
  ASSERT_EQ(1U, video_codecs.size());
  EXPECT_EQ(video_codec, video_codecs[0]);
}

// Test that the capabilities and channel creation of the Filemedia engine
// depend on the stream parameters passed to its constructor.
TEST_F(FileMediaEngineTest, TestGetCapabilities) {
  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_, "", "", "", 1));
  EXPECT_EQ(AUDIO_SEND, engine_->GetCapabilities());
  EXPECT_TRUE(NULL != voice_channel_.get());
  EXPECT_TRUE(NULL == video_channel_.get());

  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_, "", "", 1));
  EXPECT_EQ(AUDIO_SEND | AUDIO_RECV, engine_->GetCapabilities());
  EXPECT_TRUE(NULL != voice_channel_.get());
  EXPECT_TRUE(NULL == video_channel_.get());

  EXPECT_TRUE(CreateEngineAndChannels("", "", video_input_filename_, "", 1));
  EXPECT_EQ(VIDEO_SEND, engine_->GetCapabilities());
  EXPECT_TRUE(NULL == voice_channel_.get());
  EXPECT_TRUE(NULL != video_channel_.get());

  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_,
                                      video_input_filename_,
                                      video_output_filename_,
                                      1));
  EXPECT_EQ(AUDIO_SEND | AUDIO_RECV | VIDEO_SEND | VIDEO_RECV,
            engine_->GetCapabilities());
  EXPECT_TRUE(NULL != voice_channel_.get());
  EXPECT_TRUE(NULL != video_channel_.get());
}

// FileVideoChannel is the same as FileVoiceChannel in terms of receiving and
// sending the RTP packets. We therefore test only FileVoiceChannel.

// Test that SetSend() controls whether a voice channel sends RTP packets.
TEST_F(FileMediaEngineTest, TestVoiceChannelSetSend) {
  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_, "", "", 1));
  EXPECT_TRUE(NULL != voice_channel_.get());
  talk_base::MemoryStream net_dump;
  FileNetworkInterface net_interface(&net_dump, voice_channel_.get());
  voice_channel_->SetInterface(&net_interface);

  // The channel is not sending yet.
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  EXPECT_EQ(0U, net_interface.num_sent_packets());

  // The channel starts sending.
  voice_channel_->SetSend(SEND_MICROPHONE);
  EXPECT_TRUE_WAIT(net_interface.num_sent_packets() >= 1U, kWaitTimeout);

  // The channel stops sending.
  voice_channel_->SetSend(SEND_NOTHING);
  // Wait until packets are all delivered.
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  size_t old_number = net_interface.num_sent_packets();
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  EXPECT_EQ(old_number, net_interface.num_sent_packets());

  // The channel starts sending again.
  voice_channel_->SetSend(SEND_MICROPHONE);
  EXPECT_TRUE_WAIT(net_interface.num_sent_packets() > old_number, kWaitTimeout);

  // When the function exits, the net_interface object is released. The sender
  // thread may call net_interface to send packets, which results in a segment
  // fault. We hence stop sending and wait until all packets are delivered
  // before we exit this function.
  voice_channel_->SetSend(SEND_NOTHING);
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
}

// Test the sender thread of the channel. The sender sends RTP packets
// continuously with proper sequence number, timestamp, and payload.
TEST_F(FileMediaEngineTest, TestVoiceChannelSenderThread) {
  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_, "", "", 1));
  EXPECT_TRUE(NULL != voice_channel_.get());
  talk_base::MemoryStream net_dump;
  FileNetworkInterface net_interface(&net_dump, voice_channel_.get());
  voice_channel_->SetInterface(&net_interface);

  voice_channel_->SetSend(SEND_MICROPHONE);
  // Wait until the number of sent packets is no less than 2 * kPacketNumber.
  EXPECT_TRUE_WAIT(
      net_interface.num_sent_packets() >=
          2 * RtpTestUtility::GetTestPacketCount(),
      kWaitTimeout);
  voice_channel_->SetSend(SEND_NOTHING);
  // Wait until packets are all delivered.
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      2 * RtpTestUtility::GetTestPacketCount(), &net_dump,
      RtpTestUtility::kDefaultSsrc));

  // Each sent packet is dumped to net_dump and is also feed to the channel
  // via OnPacketReceived, which in turn writes the packets into voice_output_.
  // We next verify the packets in voice_output_.
  voice_channel_.reset();  // Force to close the files.
  talk_base::scoped_ptr<talk_base::StreamInterface> voice_output_;
  voice_output_.reset(talk_base::Filesystem::OpenFile(
      talk_base::Pathname(voice_output_filename_), "rb"));
  EXPECT_TRUE(voice_output_.get() != NULL);
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      2 * RtpTestUtility::GetTestPacketCount(), voice_output_.get(),
      RtpTestUtility::kDefaultSsrc));
}

// Test that we can specify the ssrc for outgoing RTP packets.
TEST_F(FileMediaEngineTest, TestVoiceChannelSendSsrc) {
  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_, "", "", 1));
  EXPECT_TRUE(NULL != voice_channel_.get());
  const uint32 send_ssrc = RtpTestUtility::kDefaultSsrc + 1;
  voice_channel_->AddSendStream(StreamParams::CreateLegacy(send_ssrc));

  talk_base::MemoryStream net_dump;
  FileNetworkInterface net_interface(&net_dump, voice_channel_.get());
  voice_channel_->SetInterface(&net_interface);

  voice_channel_->SetSend(SEND_MICROPHONE);
  // Wait until the number of sent packets is no less than 2 * kPacketNumber.
  EXPECT_TRUE_WAIT(
      net_interface.num_sent_packets() >=
          2 * RtpTestUtility::GetTestPacketCount(),
      kWaitTimeout);
  voice_channel_->SetSend(SEND_NOTHING);
  // Wait until packets are all delivered.
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      2 * RtpTestUtility::GetTestPacketCount(), &net_dump, send_ssrc));

  // Each sent packet is dumped to net_dump and is also feed to the channel
  // via OnPacketReceived, which in turn writes the packets into voice_output_.
  // We next verify the packets in voice_output_.
  voice_channel_.reset();  // Force to close the files.
  talk_base::scoped_ptr<talk_base::StreamInterface> voice_output_;
  voice_output_.reset(talk_base::Filesystem::OpenFile(
      talk_base::Pathname(voice_output_filename_), "rb"));
  EXPECT_TRUE(voice_output_.get() != NULL);
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      2 * RtpTestUtility::GetTestPacketCount(), voice_output_.get(),
      send_ssrc));
}

// Test the sender thread of the channel, where the input rtpdump has two SSRCs.
TEST_F(FileMediaEngineTest, TestVoiceChannelSenderThreadTwoSsrcs) {
  EXPECT_TRUE(CreateEngineAndChannels(voice_input_filename_,
                                      voice_output_filename_, "", "", 2));
  // Verify that voice_input_filename_ contains 2 *
  // RtpTestUtility::GetTestPacketCount() packets
  // with different SSRCs.
  talk_base::scoped_ptr<talk_base::StreamInterface> input_stream(
      talk_base::Filesystem::OpenFile(
          talk_base::Pathname(voice_input_filename_), "rb"));
  ASSERT_TRUE(NULL != input_stream.get());
  size_t ssrc_count;
  size_t packet_count;
  EXPECT_TRUE(GetSsrcAndPacketCounts(input_stream.get(), &ssrc_count,
                                     &packet_count));
  EXPECT_EQ(2U, ssrc_count);
  EXPECT_EQ(2 * RtpTestUtility::GetTestPacketCount(), packet_count);
  input_stream.reset();

  // Send 2 * RtpTestUtility::GetTestPacketCount() packets and verify that all
  // these packets have the same SSRCs (that is, the packets with different
  // SSRCs are skipped by the filemediaengine).
  EXPECT_TRUE(NULL != voice_channel_.get());
  talk_base::MemoryStream net_dump;
  FileNetworkInterface net_interface(&net_dump, voice_channel_.get());
  voice_channel_->SetInterface(&net_interface);
  voice_channel_->SetSend(SEND_MICROPHONE);
  EXPECT_TRUE_WAIT(
      net_interface.num_sent_packets() >=
          2 * RtpTestUtility::GetTestPacketCount(),
      kWaitTimeout);
  voice_channel_->SetSend(SEND_NOTHING);
  // Wait until packets are all delivered.
  talk_base::Thread::Current()->ProcessMessages(kWaitTimeMs);
  net_dump.Rewind();
  EXPECT_TRUE(GetSsrcAndPacketCounts(&net_dump, &ssrc_count, &packet_count));
  EXPECT_EQ(1U, ssrc_count);
  EXPECT_GE(packet_count, 2 * RtpTestUtility::GetTestPacketCount());
}

// Test SendIntraFrame() and RequestIntraFrame() of video channel.
TEST_F(FileMediaEngineTest, TestVideoChannelIntraFrame) {
  EXPECT_TRUE(CreateEngineAndChannels("", "", video_input_filename_,
                                      video_output_filename_, 1));
  EXPECT_TRUE(NULL != video_channel_.get());
  EXPECT_FALSE(video_channel_->SendIntraFrame());
  EXPECT_FALSE(video_channel_->RequestIntraFrame());
}

}  // namespace cricket
