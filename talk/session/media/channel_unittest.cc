/*
 * libjingle
 * Copyright 2009 Google Inc.
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

#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakertp.h"
#include "talk/media/base/fakescreencapturerfactory.h"
#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/rtpdump.h"
#include "talk/media/base/screencastid.h"
#include "talk/media/base/testutils.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "talk/session/media/channel.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/window.h"

#define MAYBE_SKIP_TEST(feature)                    \
  if (!(rtc::SSLStreamAdapter::feature())) {  \
    LOG(LS_INFO) << "Feature disabled... skipping"; \
    return;                                         \
  }

using cricket::CA_OFFER;
using cricket::CA_PRANSWER;
using cricket::CA_ANSWER;
using cricket::CA_UPDATE;
using cricket::FakeVoiceMediaChannel;
using cricket::ScreencastId;
using cricket::StreamParams;
using cricket::TransportChannel;
using rtc::WindowId;

static const cricket::AudioCodec kPcmuCodec(0, "PCMU", 64000, 8000, 1, 0);
static const cricket::AudioCodec kPcmaCodec(8, "PCMA", 64000, 8000, 1, 0);
static const cricket::AudioCodec kIsacCodec(103, "ISAC", 40000, 16000, 1, 0);
static const cricket::VideoCodec kH264Codec(97, "H264", 640, 400, 30, 0);
static const cricket::VideoCodec kH264SvcCodec(99, "H264-SVC", 320, 200, 15, 0);
static const cricket::DataCodec kGoogleDataCodec(101, "google-data", 0);
static const uint32_t kSsrc1 = 0x1111;
static const uint32_t kSsrc2 = 0x2222;
static const uint32_t kSsrc3 = 0x3333;
static const int kAudioPts[] = {0, 8};
static const int kVideoPts[] = {97, 99};

template <class ChannelT,
          class MediaChannelT,
          class ContentT,
          class CodecT,
          class MediaInfoT,
          class OptionsT>
class Traits {
 public:
  typedef ChannelT Channel;
  typedef MediaChannelT MediaChannel;
  typedef ContentT Content;
  typedef CodecT Codec;
  typedef MediaInfoT MediaInfo;
  typedef OptionsT Options;
};

// Controls how long we wait for a session to send messages that we
// expect, in milliseconds.  We put it high to avoid flaky tests.
static const int kEventTimeout = 5000;

class VoiceTraits : public Traits<cricket::VoiceChannel,
                                  cricket::FakeVoiceMediaChannel,
                                  cricket::AudioContentDescription,
                                  cricket::AudioCodec,
                                  cricket::VoiceMediaInfo,
                                  cricket::AudioOptions> {};

class VideoTraits : public Traits<cricket::VideoChannel,
                                  cricket::FakeVideoMediaChannel,
                                  cricket::VideoContentDescription,
                                  cricket::VideoCodec,
                                  cricket::VideoMediaInfo,
                                  cricket::VideoOptions> {};

class DataTraits : public Traits<cricket::DataChannel,
                                 cricket::FakeDataMediaChannel,
                                 cricket::DataContentDescription,
                                 cricket::DataCodec,
                                 cricket::DataMediaInfo,
                                 cricket::DataOptions> {};

rtc::StreamInterface* Open(const std::string& path) {
  return rtc::Filesystem::OpenFile(
      rtc::Pathname(path), "wb");
}

// Base class for Voice/VideoChannel tests
template<class T>
class ChannelTest : public testing::Test, public sigslot::has_slots<> {
 public:
  enum Flags { RTCP = 0x1, RTCP_MUX = 0x2, SECURE = 0x4, SSRC_MUX = 0x8,
               DTLS = 0x10 };

  ChannelTest(bool verify_playout,
              const uint8_t* rtp_data,
              int rtp_len,
              const uint8_t* rtcp_data,
              int rtcp_len)
      : verify_playout_(verify_playout),
        transport_controller1_(cricket::ICEROLE_CONTROLLING),
        transport_controller2_(cricket::ICEROLE_CONTROLLED),
        media_channel1_(NULL),
        media_channel2_(NULL),
        rtp_packet_(reinterpret_cast<const char*>(rtp_data), rtp_len),
        rtcp_packet_(reinterpret_cast<const char*>(rtcp_data), rtcp_len),
        media_info_callbacks1_(),
        media_info_callbacks2_() {}

  void CreateChannels(int flags1, int flags2) {
    CreateChannels(new typename T::MediaChannel(NULL, typename T::Options()),
                   new typename T::MediaChannel(NULL, typename T::Options()),
                   flags1, flags2, rtc::Thread::Current());
  }
  void CreateChannels(
      typename T::MediaChannel* ch1, typename T::MediaChannel* ch2,
      int flags1, int flags2, rtc::Thread* thread) {
    media_channel1_ = ch1;
    media_channel2_ = ch2;
    channel1_.reset(CreateChannel(thread, &media_engine_, ch1,
                                  &transport_controller1_,
                                  (flags1 & RTCP) != 0));
    channel2_.reset(CreateChannel(thread, &media_engine_, ch2,
                                  &transport_controller2_,
                                  (flags2 & RTCP) != 0));
    channel1_->SignalMediaMonitor.connect(
        this, &ChannelTest<T>::OnMediaMonitor);
    channel2_->SignalMediaMonitor.connect(
        this, &ChannelTest<T>::OnMediaMonitor);
    if ((flags1 & DTLS) && (flags2 & DTLS)) {
      flags1 = (flags1 & ~SECURE);
      flags2 = (flags2 & ~SECURE);
    }
    CreateContent(flags1, kPcmuCodec, kH264Codec,
                  &local_media_content1_);
    CreateContent(flags2, kPcmuCodec, kH264Codec,
                  &local_media_content2_);
    CopyContent(local_media_content1_, &remote_media_content1_);
    CopyContent(local_media_content2_, &remote_media_content2_);

    if (flags1 & DTLS) {
      // Confirmed to work with KT_RSA and KT_ECDSA.
      transport_controller1_.SetLocalCertificate(rtc::RTCCertificate::Create(
          rtc::scoped_ptr<rtc::SSLIdentity>(
              rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT))
              .Pass()));
    }
    if (flags2 & DTLS) {
      // Confirmed to work with KT_RSA and KT_ECDSA.
      transport_controller2_.SetLocalCertificate(rtc::RTCCertificate::Create(
          rtc::scoped_ptr<rtc::SSLIdentity>(
              rtc::SSLIdentity::Generate("session2", rtc::KT_DEFAULT))
              .Pass()));
    }

    // Add stream information (SSRC) to the local content but not to the remote
    // content. This means that we per default know the SSRC of what we send but
    // not what we receive.
    AddLegacyStreamInContent(kSsrc1, flags1, &local_media_content1_);
    AddLegacyStreamInContent(kSsrc2, flags2, &local_media_content2_);

    // If SSRC_MUX is used we also need to know the SSRC of the incoming stream.
    if (flags1 & SSRC_MUX) {
      AddLegacyStreamInContent(kSsrc1, flags1, &remote_media_content1_);
    }
    if (flags2 & SSRC_MUX) {
      AddLegacyStreamInContent(kSsrc2, flags2, &remote_media_content2_);
    }
  }
  typename T::Channel* CreateChannel(
      rtc::Thread* thread,
      cricket::MediaEngineInterface* engine,
      typename T::MediaChannel* ch,
      cricket::TransportController* transport_controller,
      bool rtcp) {
    typename T::Channel* channel = new typename T::Channel(
        thread, engine, ch, transport_controller, cricket::CN_AUDIO, rtcp);
    if (!channel->Init()) {
      delete channel;
      channel = NULL;
    }
    return channel;
  }

  bool SendInitiate() {
    bool result = channel1_->SetLocalContent(&local_media_content1_,
                                             CA_OFFER, NULL);
    if (result) {
      channel1_->Enable(true);
      result = channel2_->SetRemoteContent(&remote_media_content1_,
                                           CA_OFFER, NULL);
      if (result) {
        transport_controller1_.Connect(&transport_controller2_);

        result = channel2_->SetLocalContent(&local_media_content2_,
                                            CA_ANSWER, NULL);
      }
    }
    return result;
  }

  bool SendAccept() {
    channel2_->Enable(true);
    return channel1_->SetRemoteContent(&remote_media_content2_,
                                       CA_ANSWER, NULL);
  }

  bool SendOffer() {
    bool result = channel1_->SetLocalContent(&local_media_content1_,
                                             CA_OFFER, NULL);
    if (result) {
      channel1_->Enable(true);
      result = channel2_->SetRemoteContent(&remote_media_content1_,
                                           CA_OFFER, NULL);
    }
    return result;
  }

  bool SendProvisionalAnswer() {
    bool result = channel2_->SetLocalContent(&local_media_content2_,
                                             CA_PRANSWER, NULL);
    if (result) {
      channel2_->Enable(true);
      result = channel1_->SetRemoteContent(&remote_media_content2_,
                                           CA_PRANSWER, NULL);
      transport_controller1_.Connect(&transport_controller2_);
    }
    return result;
  }

  bool SendFinalAnswer() {
    bool result = channel2_->SetLocalContent(&local_media_content2_,
                                             CA_ANSWER, NULL);
    if (result)
      result = channel1_->SetRemoteContent(&remote_media_content2_,
                                           CA_ANSWER, NULL);
    return result;
  }

  bool SendTerminate() {
    channel1_.reset();
    channel2_.reset();
    return true;
  }

  bool AddStream1(int id) {
    return channel1_->AddRecvStream(cricket::StreamParams::CreateLegacy(id));
  }
  bool RemoveStream1(int id) {
    return channel1_->RemoveRecvStream(id);
  }

  // Calling "_w" method here is ok since we only use one thread for this test
  cricket::FakeTransport* GetTransport1() {
    return transport_controller1_.GetTransport_w(channel1_->content_name());
  }
  cricket::FakeTransport* GetTransport2() {
    return transport_controller2_.GetTransport_w(channel2_->content_name());
  }

  bool SendRtp1() {
    return media_channel1_->SendRtp(rtp_packet_.c_str(),
                                    static_cast<int>(rtp_packet_.size()),
                                    rtc::PacketOptions());
  }
  bool SendRtp2() {
    return media_channel2_->SendRtp(rtp_packet_.c_str(),
                                    static_cast<int>(rtp_packet_.size()),
                                    rtc::PacketOptions());
  }
  bool SendRtcp1() {
    return media_channel1_->SendRtcp(rtcp_packet_.c_str(),
                                     static_cast<int>(rtcp_packet_.size()));
  }
  bool SendRtcp2() {
    return media_channel2_->SendRtcp(rtcp_packet_.c_str(),
                                     static_cast<int>(rtcp_packet_.size()));
  }
  // Methods to send custom data.
  bool SendCustomRtp1(uint32_t ssrc, int sequence_number, int pl_type = -1) {
    std::string data(CreateRtpData(ssrc, sequence_number, pl_type));
    return media_channel1_->SendRtp(data.c_str(), static_cast<int>(data.size()),
                                    rtc::PacketOptions());
  }
  bool SendCustomRtp2(uint32_t ssrc, int sequence_number, int pl_type = -1) {
    std::string data(CreateRtpData(ssrc, sequence_number, pl_type));
    return media_channel2_->SendRtp(data.c_str(), static_cast<int>(data.size()),
                                    rtc::PacketOptions());
  }
  bool SendCustomRtcp1(uint32_t ssrc) {
    std::string data(CreateRtcpData(ssrc));
    return media_channel1_->SendRtcp(data.c_str(),
                                     static_cast<int>(data.size()));
  }
  bool SendCustomRtcp2(uint32_t ssrc) {
    std::string data(CreateRtcpData(ssrc));
    return media_channel2_->SendRtcp(data.c_str(),
                                     static_cast<int>(data.size()));
  }
  bool CheckRtp1() {
    return media_channel1_->CheckRtp(rtp_packet_.c_str(),
                                     static_cast<int>(rtp_packet_.size()));
  }
  bool CheckRtp2() {
    return media_channel2_->CheckRtp(rtp_packet_.c_str(),
                                     static_cast<int>(rtp_packet_.size()));
  }
  bool CheckRtcp1() {
    return media_channel1_->CheckRtcp(rtcp_packet_.c_str(),
                                      static_cast<int>(rtcp_packet_.size()));
  }
  bool CheckRtcp2() {
    return media_channel2_->CheckRtcp(rtcp_packet_.c_str(),
                                      static_cast<int>(rtcp_packet_.size()));
  }
  // Methods to check custom data.
  bool CheckCustomRtp1(uint32_t ssrc, int sequence_number, int pl_type = -1) {
    std::string data(CreateRtpData(ssrc, sequence_number, pl_type));
    return media_channel1_->CheckRtp(data.c_str(),
                                     static_cast<int>(data.size()));
  }
  bool CheckCustomRtp2(uint32_t ssrc, int sequence_number, int pl_type = -1) {
    std::string data(CreateRtpData(ssrc, sequence_number, pl_type));
    return media_channel2_->CheckRtp(data.c_str(),
                                     static_cast<int>(data.size()));
  }
  bool CheckCustomRtcp1(uint32_t ssrc) {
    std::string data(CreateRtcpData(ssrc));
    return media_channel1_->CheckRtcp(data.c_str(),
                                      static_cast<int>(data.size()));
  }
  bool CheckCustomRtcp2(uint32_t ssrc) {
    std::string data(CreateRtcpData(ssrc));
    return media_channel2_->CheckRtcp(data.c_str(),
                                      static_cast<int>(data.size()));
  }
  std::string CreateRtpData(uint32_t ssrc, int sequence_number, int pl_type) {
    std::string data(rtp_packet_);
    // Set SSRC in the rtp packet copy.
    rtc::SetBE32(const_cast<char*>(data.c_str()) + 8, ssrc);
    rtc::SetBE16(const_cast<char*>(data.c_str()) + 2, sequence_number);
    if (pl_type >= 0) {
      rtc::Set8(const_cast<char*>(data.c_str()), 1,
                static_cast<uint8_t>(pl_type));
    }
    return data;
  }
  std::string CreateRtcpData(uint32_t ssrc) {
    std::string data(rtcp_packet_);
    // Set SSRC in the rtcp packet copy.
    rtc::SetBE32(const_cast<char*>(data.c_str()) + 4, ssrc);
    return data;
  }

  bool CheckNoRtp1() {
    return media_channel1_->CheckNoRtp();
  }
  bool CheckNoRtp2() {
    return media_channel2_->CheckNoRtp();
  }
  bool CheckNoRtcp1() {
    return media_channel1_->CheckNoRtcp();
  }
  bool CheckNoRtcp2() {
    return media_channel2_->CheckNoRtcp();
  }

  void CreateContent(int flags,
                     const cricket::AudioCodec& audio_codec,
                     const cricket::VideoCodec& video_codec,
                     typename T::Content* content) {
    // overridden in specialized classes
  }
  void CopyContent(const typename T::Content& source,
                   typename T::Content* content) {
    // overridden in specialized classes
  }

  // Creates a cricket::SessionDescription with one MediaContent and one stream.
  // kPcmuCodec is used as audio codec and kH264Codec is used as video codec.
  cricket::SessionDescription* CreateSessionDescriptionWithStream(
      uint32_t ssrc) {
     typename T::Content content;
     cricket::SessionDescription* sdesc = new cricket::SessionDescription();
     CreateContent(SECURE, kPcmuCodec, kH264Codec, &content);
     AddLegacyStreamInContent(ssrc, 0, &content);
     sdesc->AddContent("DUMMY_CONTENT_NAME",
                       cricket::NS_JINGLE_RTP, content.Copy());
     return sdesc;
  }

  class CallThread : public rtc::SignalThread {
   public:
    typedef bool (ChannelTest<T>::*Method)();
    CallThread(ChannelTest<T>* obj, Method method, bool* result)
        : obj_(obj),
          method_(method),
          result_(result) {
      *result = false;
    }
    virtual void DoWork() {
      bool result = (*obj_.*method_)();
      if (result_) {
        *result_ = result;
      }
    }
   private:
    ChannelTest<T>* obj_;
    Method method_;
    bool* result_;
  };
  void CallOnThread(typename CallThread::Method method, bool* result) {
    CallThread* thread = new CallThread(this, method, result);
    thread->Start();
    thread->Release();
  }

  void CallOnThreadAndWaitForDone(typename CallThread::Method method,
                                  bool* result) {
    CallThread* thread = new CallThread(this, method, result);
    thread->Start();
    thread->Destroy(true);
  }

  bool CodecMatches(const typename T::Codec& c1, const typename T::Codec& c2) {
    return false;  // overridden in specialized classes
  }

  void OnMediaMonitor(typename T::Channel* channel,
                      const typename T::MediaInfo& info) {
    if (channel == channel1_.get()) {
      media_info_callbacks1_++;
    } else if (channel == channel2_.get()) {
      media_info_callbacks2_++;
    }
  }

  void AddLegacyStreamInContent(uint32_t ssrc,
                                int flags,
                                typename T::Content* content) {
    // Base implementation.
  }

  // Tests that can be used by derived classes.

  // Basic sanity check.
  void TestInit() {
    CreateChannels(0, 0);
    EXPECT_FALSE(channel1_->secure());
    EXPECT_FALSE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_FALSE(media_channel1_->playout());
    }
    EXPECT_TRUE(media_channel1_->codecs().empty());
    EXPECT_TRUE(media_channel1_->recv_streams().empty());
    EXPECT_TRUE(media_channel1_->rtp_packets().empty());
    EXPECT_TRUE(media_channel1_->rtcp_packets().empty());
  }

  // Test that SetLocalContent and SetRemoteContent properly configure
  // the codecs.
  void TestSetContents() {
    CreateChannels(0, 0);
    typename T::Content content;
    CreateContent(0, kPcmuCodec, kH264Codec, &content);
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    EXPECT_EQ(0U, media_channel1_->codecs().size());
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    ASSERT_EQ(1U, media_channel1_->codecs().size());
    EXPECT_TRUE(CodecMatches(content.codecs()[0],
                             media_channel1_->codecs()[0]));
  }

  // Test that SetLocalContent and SetRemoteContent properly deals
  // with an empty offer.
  void TestSetContentsNullOffer() {
    CreateChannels(0, 0);
    typename T::Content content;
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    CreateContent(0, kPcmuCodec, kH264Codec, &content);
    EXPECT_EQ(0U, media_channel1_->codecs().size());
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    ASSERT_EQ(1U, media_channel1_->codecs().size());
    EXPECT_TRUE(CodecMatches(content.codecs()[0],
                             media_channel1_->codecs()[0]));
  }

  // Test that SetLocalContent and SetRemoteContent properly set RTCP
  // mux.
  void TestSetContentsRtcpMux() {
    CreateChannels(RTCP, RTCP);
    EXPECT_TRUE(channel1_->rtcp_transport_channel() != NULL);
    EXPECT_TRUE(channel2_->rtcp_transport_channel() != NULL);
    typename T::Content content;
    CreateContent(0, kPcmuCodec, kH264Codec, &content);
    // Both sides agree on mux. Should no longer be a separate RTCP channel.
    content.set_rtcp_mux(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    EXPECT_TRUE(channel1_->rtcp_transport_channel() == NULL);
    // Only initiator supports mux. Should still have a separate RTCP channel.
    EXPECT_TRUE(channel2_->SetLocalContent(&content, CA_OFFER, NULL));
    content.set_rtcp_mux(false);
    EXPECT_TRUE(channel2_->SetRemoteContent(&content, CA_ANSWER, NULL));
    EXPECT_TRUE(channel2_->rtcp_transport_channel() != NULL);
  }

  // Test that SetLocalContent and SetRemoteContent properly set RTCP
  // mux when a provisional answer is received.
  void TestSetContentsRtcpMuxWithPrAnswer() {
    CreateChannels(RTCP, RTCP);
    EXPECT_TRUE(channel1_->rtcp_transport_channel() != NULL);
    EXPECT_TRUE(channel2_->rtcp_transport_channel() != NULL);
    typename T::Content content;
    CreateContent(0, kPcmuCodec, kH264Codec, &content);
    content.set_rtcp_mux(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_PRANSWER, NULL));
    EXPECT_TRUE(channel1_->rtcp_transport_channel() != NULL);
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    // Both sides agree on mux. Should no longer be a separate RTCP channel.
    EXPECT_TRUE(channel1_->rtcp_transport_channel() == NULL);
    // Only initiator supports mux. Should still have a separate RTCP channel.
    EXPECT_TRUE(channel2_->SetLocalContent(&content, CA_OFFER, NULL));
    content.set_rtcp_mux(false);
    EXPECT_TRUE(channel2_->SetRemoteContent(&content, CA_PRANSWER, NULL));
    EXPECT_TRUE(channel2_->SetRemoteContent(&content, CA_ANSWER, NULL));
    EXPECT_TRUE(channel2_->rtcp_transport_channel() != NULL);
  }

  // Test that SetRemoteContent properly deals with a content update.
  void TestSetRemoteContentUpdate() {
    CreateChannels(0, 0);
    typename T::Content content;
    CreateContent(RTCP | RTCP_MUX | SECURE,
                  kPcmuCodec, kH264Codec,
                  &content);
    EXPECT_EQ(0U, media_channel1_->codecs().size());
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    ASSERT_EQ(1U, media_channel1_->codecs().size());
    EXPECT_TRUE(CodecMatches(content.codecs()[0],
                             media_channel1_->codecs()[0]));
    // Now update with other codecs.
    typename T::Content update_content;
    update_content.set_partial(true);
    CreateContent(0, kIsacCodec, kH264SvcCodec,
                  &update_content);
    EXPECT_TRUE(channel1_->SetRemoteContent(&update_content, CA_UPDATE, NULL));
    ASSERT_EQ(1U, media_channel1_->codecs().size());
    EXPECT_TRUE(CodecMatches(update_content.codecs()[0],
                             media_channel1_->codecs()[0]));
    // Now update without any codecs. This is ignored.
    typename T::Content empty_content;
    empty_content.set_partial(true);
    EXPECT_TRUE(channel1_->SetRemoteContent(&empty_content, CA_UPDATE, NULL));
    ASSERT_EQ(1U, media_channel1_->codecs().size());
    EXPECT_TRUE(CodecMatches(update_content.codecs()[0],
                             media_channel1_->codecs()[0]));
  }

  // Test that Add/RemoveStream properly forward to the media channel.
  void TestStreams() {
    CreateChannels(0, 0);
    EXPECT_TRUE(AddStream1(1));
    EXPECT_TRUE(AddStream1(2));
    EXPECT_EQ(2U, media_channel1_->recv_streams().size());
    EXPECT_TRUE(RemoveStream1(2));
    EXPECT_EQ(1U, media_channel1_->recv_streams().size());
    EXPECT_TRUE(RemoveStream1(1));
    EXPECT_EQ(0U, media_channel1_->recv_streams().size());
  }

  // Test that SetLocalContent properly handles adding and removing StreamParams
  // to the local content description.
  // This test uses the CA_UPDATE action that don't require a full
  // MediaContentDescription to do an update.
  void TestUpdateStreamsInLocalContent() {
    cricket::StreamParams stream1;
    stream1.groupid = "group1";
    stream1.id = "stream1";
    stream1.ssrcs.push_back(kSsrc1);
    stream1.cname = "stream1_cname";

    cricket::StreamParams stream2;
    stream2.groupid = "group2";
    stream2.id = "stream2";
    stream2.ssrcs.push_back(kSsrc2);
    stream2.cname = "stream2_cname";

    cricket::StreamParams stream3;
    stream3.groupid = "group3";
    stream3.id = "stream3";
    stream3.ssrcs.push_back(kSsrc3);
    stream3.cname = "stream3_cname";

    CreateChannels(0, 0);
    typename T::Content content1;
    CreateContent(0, kPcmuCodec, kH264Codec, &content1);
    content1.AddStream(stream1);
    EXPECT_EQ(0u, media_channel1_->send_streams().size());
    EXPECT_TRUE(channel1_->SetLocalContent(&content1, CA_OFFER, NULL));

    ASSERT_EQ(1u, media_channel1_->send_streams().size());
    EXPECT_EQ(stream1, media_channel1_->send_streams()[0]);

    // Update the local streams by adding another sending stream.
    // Use a partial updated session description.
    typename T::Content content2;
    content2.AddStream(stream2);
    content2.AddStream(stream3);
    content2.set_partial(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content2, CA_UPDATE, NULL));
    ASSERT_EQ(3u, media_channel1_->send_streams().size());
    EXPECT_EQ(stream1, media_channel1_->send_streams()[0]);
    EXPECT_EQ(stream2, media_channel1_->send_streams()[1]);
    EXPECT_EQ(stream3, media_channel1_->send_streams()[2]);

    // Update the local streams by removing the first sending stream.
    // This is done by removing all SSRCS for this particular stream.
    typename T::Content content3;
    stream1.ssrcs.clear();
    content3.AddStream(stream1);
    content3.set_partial(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content3, CA_UPDATE, NULL));
    ASSERT_EQ(2u, media_channel1_->send_streams().size());
    EXPECT_EQ(stream2, media_channel1_->send_streams()[0]);
    EXPECT_EQ(stream3, media_channel1_->send_streams()[1]);

    // Update the local streams with a stream that does not change.
    // THe update is ignored.
    typename T::Content content4;
    content4.AddStream(stream2);
    content4.set_partial(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content4, CA_UPDATE, NULL));
    ASSERT_EQ(2u, media_channel1_->send_streams().size());
    EXPECT_EQ(stream2, media_channel1_->send_streams()[0]);
    EXPECT_EQ(stream3, media_channel1_->send_streams()[1]);
  }

  // Test that SetRemoteContent properly handles adding and removing
  // StreamParams to the remote content description.
  // This test uses the CA_UPDATE action that don't require a full
  // MediaContentDescription to do an update.
  void TestUpdateStreamsInRemoteContent() {
    cricket::StreamParams stream1;
    stream1.id = "Stream1";
    stream1.groupid = "1";
    stream1.ssrcs.push_back(kSsrc1);
    stream1.cname = "stream1_cname";

    cricket::StreamParams stream2;
    stream2.id = "Stream2";
    stream2.groupid = "2";
    stream2.ssrcs.push_back(kSsrc2);
    stream2.cname = "stream2_cname";

    cricket::StreamParams stream3;
    stream3.id = "Stream3";
    stream3.groupid = "3";
    stream3.ssrcs.push_back(kSsrc3);
    stream3.cname = "stream3_cname";

    CreateChannels(0, 0);
    typename T::Content content1;
    CreateContent(0, kPcmuCodec, kH264Codec, &content1);
    content1.AddStream(stream1);
    EXPECT_EQ(0u, media_channel1_->recv_streams().size());
    EXPECT_TRUE(channel1_->SetRemoteContent(&content1, CA_OFFER, NULL));

    ASSERT_EQ(1u, media_channel1_->codecs().size());
    ASSERT_EQ(1u, media_channel1_->recv_streams().size());
    EXPECT_EQ(stream1, media_channel1_->recv_streams()[0]);

    // Update the remote streams by adding another sending stream.
    // Use a partial updated session description.
    typename T::Content content2;
    content2.AddStream(stream2);
    content2.AddStream(stream3);
    content2.set_partial(true);
    EXPECT_TRUE(channel1_->SetRemoteContent(&content2, CA_UPDATE, NULL));
    ASSERT_EQ(3u, media_channel1_->recv_streams().size());
    EXPECT_EQ(stream1, media_channel1_->recv_streams()[0]);
    EXPECT_EQ(stream2, media_channel1_->recv_streams()[1]);
    EXPECT_EQ(stream3, media_channel1_->recv_streams()[2]);

    // Update the remote streams by removing the first stream.
    // This is done by removing all SSRCS for this particular stream.
    typename T::Content content3;
    stream1.ssrcs.clear();
    content3.AddStream(stream1);
    content3.set_partial(true);
    EXPECT_TRUE(channel1_->SetRemoteContent(&content3, CA_UPDATE, NULL));
    ASSERT_EQ(2u, media_channel1_->recv_streams().size());
    EXPECT_EQ(stream2, media_channel1_->recv_streams()[0]);
    EXPECT_EQ(stream3, media_channel1_->recv_streams()[1]);

    // Update the remote streams with a stream that does not change.
    // The update is ignored.
    typename T::Content content4;
    content4.AddStream(stream2);
    content4.set_partial(true);
    EXPECT_TRUE(channel1_->SetRemoteContent(&content4, CA_UPDATE, NULL));
    ASSERT_EQ(2u, media_channel1_->recv_streams().size());
    EXPECT_EQ(stream2, media_channel1_->recv_streams()[0]);
    EXPECT_EQ(stream3, media_channel1_->recv_streams()[1]);
  }

  // Test that SetLocalContent and SetRemoteContent properly
  // handles adding and removing StreamParams when the action is a full
  // CA_OFFER / CA_ANSWER.
  void TestChangeStreamParamsInContent() {
    cricket::StreamParams stream1;
    stream1.groupid = "group1";
    stream1.id = "stream1";
    stream1.ssrcs.push_back(kSsrc1);
    stream1.cname = "stream1_cname";

    cricket::StreamParams stream2;
    stream2.groupid = "group1";
    stream2.id = "stream2";
    stream2.ssrcs.push_back(kSsrc2);
    stream2.cname = "stream2_cname";

    // Setup a call where channel 1 send |stream1| to channel 2.
    CreateChannels(0, 0);
    typename T::Content content1;
    CreateContent(0, kPcmuCodec, kH264Codec, &content1);
    content1.AddStream(stream1);
    EXPECT_TRUE(channel1_->SetLocalContent(&content1, CA_OFFER, NULL));
    EXPECT_TRUE(channel1_->Enable(true));
    EXPECT_EQ(1u, media_channel1_->send_streams().size());

    EXPECT_TRUE(channel2_->SetRemoteContent(&content1, CA_OFFER, NULL));
    EXPECT_EQ(1u, media_channel2_->recv_streams().size());
    transport_controller1_.Connect(&transport_controller2_);

    // Channel 2 do not send anything.
    typename T::Content content2;
    CreateContent(0, kPcmuCodec, kH264Codec, &content2);
    EXPECT_TRUE(channel1_->SetRemoteContent(&content2, CA_ANSWER, NULL));
    EXPECT_EQ(0u, media_channel1_->recv_streams().size());
    EXPECT_TRUE(channel2_->SetLocalContent(&content2, CA_ANSWER, NULL));
    EXPECT_TRUE(channel2_->Enable(true));
    EXPECT_EQ(0u, media_channel2_->send_streams().size());

    EXPECT_TRUE(SendCustomRtp1(kSsrc1, 0));
    EXPECT_TRUE(CheckCustomRtp2(kSsrc1, 0));

    // Let channel 2 update the content by sending |stream2| and enable SRTP.
    typename T::Content content3;
    CreateContent(SECURE, kPcmuCodec, kH264Codec, &content3);
    content3.AddStream(stream2);
    EXPECT_TRUE(channel2_->SetLocalContent(&content3, CA_OFFER, NULL));
    ASSERT_EQ(1u, media_channel2_->send_streams().size());
    EXPECT_EQ(stream2, media_channel2_->send_streams()[0]);

    EXPECT_TRUE(channel1_->SetRemoteContent(&content3, CA_OFFER, NULL));
    ASSERT_EQ(1u, media_channel1_->recv_streams().size());
    EXPECT_EQ(stream2, media_channel1_->recv_streams()[0]);

    // Channel 1 replies but stop sending stream1.
    typename T::Content content4;
    CreateContent(SECURE, kPcmuCodec, kH264Codec, &content4);
    EXPECT_TRUE(channel1_->SetLocalContent(&content4, CA_ANSWER, NULL));
    EXPECT_EQ(0u, media_channel1_->send_streams().size());

    EXPECT_TRUE(channel2_->SetRemoteContent(&content4, CA_ANSWER, NULL));
    EXPECT_EQ(0u, media_channel2_->recv_streams().size());

    EXPECT_TRUE(channel1_->secure());
    EXPECT_TRUE(channel2_->secure());
    EXPECT_TRUE(SendCustomRtp2(kSsrc2, 0));
    EXPECT_TRUE(CheckCustomRtp1(kSsrc2, 0));
  }

  // Test that we only start playout and sending at the right times.
  void TestPlayoutAndSendingStates() {
    CreateChannels(0, 0);
    if (verify_playout_) {
      EXPECT_FALSE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());
    }
    EXPECT_FALSE(media_channel2_->sending());
    EXPECT_TRUE(channel1_->Enable(true));
    if (verify_playout_) {
      EXPECT_FALSE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    EXPECT_TRUE(channel1_->SetLocalContent(&local_media_content1_,
                                           CA_OFFER, NULL));
    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    EXPECT_TRUE(channel2_->SetRemoteContent(&local_media_content1_,
                                            CA_OFFER, NULL));
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());
    }
    EXPECT_FALSE(media_channel2_->sending());
    EXPECT_TRUE(channel2_->SetLocalContent(&local_media_content2_,
                                           CA_ANSWER, NULL));
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());
    }
    EXPECT_FALSE(media_channel2_->sending());
    transport_controller1_.Connect(&transport_controller2_);
    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());
    }
    EXPECT_FALSE(media_channel2_->sending());
    EXPECT_TRUE(channel2_->Enable(true));
    if (verify_playout_) {
      EXPECT_TRUE(media_channel2_->playout());
    }
    EXPECT_TRUE(media_channel2_->sending());
    EXPECT_TRUE(channel1_->SetRemoteContent(&local_media_content2_,
                                            CA_ANSWER, NULL));
    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_TRUE(media_channel1_->sending());
  }

  // Test that changing the MediaContentDirection in the local and remote
  // session description start playout and sending at the right time.
  void TestMediaContentDirection() {
    CreateChannels(0, 0);
    typename T::Content content1;
    CreateContent(0, kPcmuCodec, kH264Codec, &content1);
    typename T::Content content2;
    CreateContent(0, kPcmuCodec, kH264Codec, &content2);
    // Set |content2| to be InActive.
    content2.set_direction(cricket::MD_INACTIVE);

    EXPECT_TRUE(channel1_->Enable(true));
    EXPECT_TRUE(channel2_->Enable(true));
    if (verify_playout_) {
      EXPECT_FALSE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());
    }
    EXPECT_FALSE(media_channel2_->sending());

    EXPECT_TRUE(channel1_->SetLocalContent(&content1, CA_OFFER, NULL));
    EXPECT_TRUE(channel2_->SetRemoteContent(&content1, CA_OFFER, NULL));
    EXPECT_TRUE(channel2_->SetLocalContent(&content2, CA_PRANSWER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content2, CA_PRANSWER, NULL));
    transport_controller1_.Connect(&transport_controller2_);

    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());  // remote InActive
    if (verify_playout_) {
      EXPECT_FALSE(media_channel2_->playout());  // local InActive
    }
    EXPECT_FALSE(media_channel2_->sending());  // local InActive

    // Update |content2| to be RecvOnly.
    content2.set_direction(cricket::MD_RECVONLY);
    EXPECT_TRUE(channel2_->SetLocalContent(&content2, CA_PRANSWER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content2, CA_PRANSWER, NULL));

    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_TRUE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_TRUE(media_channel2_->playout());  // local RecvOnly
    }
    EXPECT_FALSE(media_channel2_->sending());  // local RecvOnly

    // Update |content2| to be SendRecv.
    content2.set_direction(cricket::MD_SENDRECV);
    EXPECT_TRUE(channel2_->SetLocalContent(&content2, CA_ANSWER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content2, CA_ANSWER, NULL));

    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_TRUE(media_channel1_->sending());
    if (verify_playout_) {
      EXPECT_TRUE(media_channel2_->playout());
    }
    EXPECT_TRUE(media_channel2_->sending());
  }

  // Test setting up a call.
  void TestCallSetup() {
    CreateChannels(0, 0);
    EXPECT_FALSE(channel1_->secure());
    EXPECT_TRUE(SendInitiate());
    if (verify_playout_) {
      EXPECT_TRUE(media_channel1_->playout());
    }
    EXPECT_FALSE(media_channel1_->sending());
    EXPECT_TRUE(SendAccept());
    EXPECT_FALSE(channel1_->secure());
    EXPECT_TRUE(media_channel1_->sending());
    EXPECT_EQ(1U, media_channel1_->codecs().size());
    if (verify_playout_) {
      EXPECT_TRUE(media_channel2_->playout());
    }
    EXPECT_TRUE(media_channel2_->sending());
    EXPECT_EQ(1U, media_channel2_->codecs().size());
  }

  // Test that we don't crash if packets are sent during call teardown
  // when RTCP mux is enabled. This is a regression test against a specific
  // race condition that would only occur when a RTCP packet was sent during
  // teardown of a channel on which RTCP mux was enabled.
  void TestCallTeardownRtcpMux() {
    class LastWordMediaChannel : public T::MediaChannel {
     public:
      LastWordMediaChannel() : T::MediaChannel(NULL, typename T::Options()) {}
      ~LastWordMediaChannel() {
        T::MediaChannel::SendRtp(kPcmuFrame, sizeof(kPcmuFrame),
                                 rtc::PacketOptions());
        T::MediaChannel::SendRtcp(kRtcpReport, sizeof(kRtcpReport));
      }
    };
    CreateChannels(new LastWordMediaChannel(), new LastWordMediaChannel(),
                   RTCP | RTCP_MUX, RTCP | RTCP_MUX,
                   rtc::Thread::Current());
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    EXPECT_TRUE(SendTerminate());
  }

  // Send voice RTP data to the other side and ensure it gets there.
  void SendRtpToRtp() {
    CreateChannels(0, 0);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
  }

  // Check that RTCP is not transmitted if both sides don't support RTCP.
  void SendNoRtcpToNoRtcp() {
    CreateChannels(0, 0);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_FALSE(SendRtcp1());
    EXPECT_FALSE(SendRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTCP is not transmitted if the callee doesn't support RTCP.
  void SendNoRtcpToRtcp() {
    CreateChannels(0, RTCP);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());
    EXPECT_FALSE(SendRtcp1());
    EXPECT_FALSE(SendRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTCP is not transmitted if the caller doesn't support RTCP.
  void SendRtcpToNoRtcp() {
    CreateChannels(RTCP, 0);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_FALSE(SendRtcp1());
    EXPECT_FALSE(SendRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTCP is transmitted if both sides support RTCP.
  void SendRtcpToRtcp() {
    CreateChannels(RTCP, RTCP);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTCP is transmitted if only the initiator supports mux.
  void SendRtcpMuxToRtcp() {
    CreateChannels(RTCP | RTCP_MUX, RTCP);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTP and RTCP are transmitted ok when both sides support mux.
  void SendRtcpMuxToRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP | RTCP_MUX);
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTP and RTCP are transmitted ok when both sides
  // support mux and one the offerer requires mux.
  void SendRequireRtcpMuxToRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP | RTCP_MUX);
    channel1_->ActivateRtcpMux();
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendAccept());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTP and RTCP are transmitted ok when both sides
  // support mux and one the answerer requires rtcp mux.
  void SendRtcpMuxToRequireRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP | RTCP_MUX);
    channel2_->ActivateRtcpMux();
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that RTP and RTCP are transmitted ok when both sides
  // require mux.
  void SendRequireRtcpMuxToRequireRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP | RTCP_MUX);
    channel1_->ActivateRtcpMux();
    channel2_->ActivateRtcpMux();
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Check that SendAccept fails if the answerer doesn't support mux
  // and the offerer requires it.
  void SendRequireRtcpMuxToNoRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP);
    channel1_->ActivateRtcpMux();
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());
    EXPECT_FALSE(SendAccept());
  }

  // Check that RTCP data sent by the initiator before the accept is not muxed.
  void SendEarlyRtcpMuxToRtcp() {
    CreateChannels(RTCP | RTCP_MUX, RTCP);
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());

    // RTCP can be sent before the call is accepted, if the transport is ready.
    // It should not be muxed though, as the remote side doesn't support mux.
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp2());

    // Send RTCP packet from callee and verify that it is received.
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckRtcp1());

    // Complete call setup and ensure everything is still OK.
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtcp1());
  }


  // Check that RTCP data is not muxed until both sides have enabled muxing,
  // but that we properly demux before we get the accept message, since there
  // is a race between RTP data and the jingle accept.
  void SendEarlyRtcpMuxToRtcpMux() {
    CreateChannels(RTCP | RTCP_MUX, RTCP | RTCP_MUX);
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());

    // RTCP can't be sent yet, since the RTCP transport isn't writable, and
    // we haven't yet received the accept that says we should mux.
    EXPECT_FALSE(SendRtcp1());

    // Send muxed RTCP packet from callee and verify that it is received.
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckRtcp1());

    // Complete call setup and ensure everything is still OK.
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtcp1());
  }

  // Test that we properly send SRTP with RTCP in both directions.
  // You can pass in DTLS and/or RTCP_MUX as flags.
  void SendSrtpToSrtp(int flags1_in = 0, int flags2_in = 0) {
    ASSERT((flags1_in & ~(RTCP_MUX | DTLS)) == 0);
    ASSERT((flags2_in & ~(RTCP_MUX | DTLS)) == 0);

    int flags1 = RTCP | SECURE | flags1_in;
    int flags2 = RTCP | SECURE | flags2_in;
    bool dtls1 = !!(flags1_in & DTLS);
    bool dtls2 = !!(flags2_in & DTLS);
    CreateChannels(flags1, flags2);
    EXPECT_FALSE(channel1_->secure());
    EXPECT_FALSE(channel2_->secure());
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE_WAIT(channel1_->writable(), kEventTimeout);
    EXPECT_TRUE_WAIT(channel2_->writable(), kEventTimeout);
    EXPECT_TRUE(SendAccept());
    EXPECT_TRUE(channel1_->secure());
    EXPECT_TRUE(channel2_->secure());
    EXPECT_EQ(dtls1 && dtls2, channel1_->secure_dtls());
    EXPECT_EQ(dtls1 && dtls2, channel2_->secure_dtls());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Test that we properly handling SRTP negotiating down to RTP.
  void SendSrtpToRtp() {
    CreateChannels(RTCP | SECURE, RTCP);
    EXPECT_FALSE(channel1_->secure());
    EXPECT_FALSE(channel2_->secure());
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    EXPECT_FALSE(channel1_->secure());
    EXPECT_FALSE(channel2_->secure());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(SendRtcp1());
    EXPECT_TRUE(SendRtcp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE(CheckRtcp1());
    EXPECT_TRUE(CheckRtcp2());
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Test that we can send and receive early media when a provisional answer is
  // sent and received. The test uses SRTP, RTCP mux and SSRC mux.
  void SendEarlyMediaUsingRtcpMuxSrtp() {
      int sequence_number1_1 = 0, sequence_number2_2 = 0;

      CreateChannels(SSRC_MUX | RTCP | RTCP_MUX | SECURE,
                     SSRC_MUX | RTCP | RTCP_MUX | SECURE);
      EXPECT_TRUE(SendOffer());
      EXPECT_TRUE(SendProvisionalAnswer());
      EXPECT_TRUE(channel1_->secure());
      EXPECT_TRUE(channel2_->secure());
      ASSERT_TRUE(GetTransport1());
      ASSERT_TRUE(GetTransport2());
      EXPECT_EQ(2U, GetTransport1()->channels().size());
      EXPECT_EQ(2U, GetTransport2()->channels().size());
      EXPECT_TRUE(SendCustomRtcp1(kSsrc1));
      EXPECT_TRUE(CheckCustomRtcp2(kSsrc1));
      EXPECT_TRUE(SendCustomRtp1(kSsrc1, ++sequence_number1_1));
      EXPECT_TRUE(CheckCustomRtp2(kSsrc1, sequence_number1_1));

      // Send packets from callee and verify that it is received.
      EXPECT_TRUE(SendCustomRtcp2(kSsrc2));
      EXPECT_TRUE(CheckCustomRtcp1(kSsrc2));
      EXPECT_TRUE(SendCustomRtp2(kSsrc2, ++sequence_number2_2));
      EXPECT_TRUE(CheckCustomRtp1(kSsrc2, sequence_number2_2));

      // Complete call setup and ensure everything is still OK.
      EXPECT_TRUE(SendFinalAnswer());
      EXPECT_EQ(1U, GetTransport1()->channels().size());
      EXPECT_EQ(1U, GetTransport2()->channels().size());
      EXPECT_TRUE(channel1_->secure());
      EXPECT_TRUE(channel2_->secure());
      EXPECT_TRUE(SendCustomRtcp1(kSsrc1));
      EXPECT_TRUE(CheckCustomRtcp2(kSsrc1));
      EXPECT_TRUE(SendCustomRtp1(kSsrc1, ++sequence_number1_1));
      EXPECT_TRUE(CheckCustomRtp2(kSsrc1, sequence_number1_1));
      EXPECT_TRUE(SendCustomRtcp2(kSsrc2));
      EXPECT_TRUE(CheckCustomRtcp1(kSsrc2));
      EXPECT_TRUE(SendCustomRtp2(kSsrc2, ++sequence_number2_2));
      EXPECT_TRUE(CheckCustomRtp1(kSsrc2, sequence_number2_2));
  }

  // Test that we properly send RTP without SRTP from a thread.
  void SendRtpToRtpOnThread() {
    bool sent_rtp1, sent_rtp2, sent_rtcp1, sent_rtcp2;
    CreateChannels(RTCP, RTCP);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    CallOnThread(&ChannelTest<T>::SendRtp1, &sent_rtp1);
    CallOnThread(&ChannelTest<T>::SendRtp2, &sent_rtp2);
    CallOnThread(&ChannelTest<T>::SendRtcp1, &sent_rtcp1);
    CallOnThread(&ChannelTest<T>::SendRtcp2, &sent_rtcp2);
    EXPECT_TRUE_WAIT(CheckRtp1(), 1000);
    EXPECT_TRUE_WAIT(CheckRtp2(), 1000);
    EXPECT_TRUE_WAIT(sent_rtp1, 1000);
    EXPECT_TRUE_WAIT(sent_rtp2, 1000);
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE_WAIT(CheckRtcp1(), 1000);
    EXPECT_TRUE_WAIT(CheckRtcp2(), 1000);
    EXPECT_TRUE_WAIT(sent_rtcp1, 1000);
    EXPECT_TRUE_WAIT(sent_rtcp2, 1000);
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Test that we properly send SRTP with RTCP from a thread.
  void SendSrtpToSrtpOnThread() {
    bool sent_rtp1, sent_rtp2, sent_rtcp1, sent_rtcp2;
    CreateChannels(RTCP | SECURE, RTCP | SECURE);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    CallOnThread(&ChannelTest<T>::SendRtp1, &sent_rtp1);
    CallOnThread(&ChannelTest<T>::SendRtp2, &sent_rtp2);
    CallOnThread(&ChannelTest<T>::SendRtcp1, &sent_rtcp1);
    CallOnThread(&ChannelTest<T>::SendRtcp2, &sent_rtcp2);
    EXPECT_TRUE_WAIT(CheckRtp1(), 1000);
    EXPECT_TRUE_WAIT(CheckRtp2(), 1000);
    EXPECT_TRUE_WAIT(sent_rtp1, 1000);
    EXPECT_TRUE_WAIT(sent_rtp2, 1000);
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
    EXPECT_TRUE_WAIT(CheckRtcp1(), 1000);
    EXPECT_TRUE_WAIT(CheckRtcp2(), 1000);
    EXPECT_TRUE_WAIT(sent_rtcp1, 1000);
    EXPECT_TRUE_WAIT(sent_rtcp2, 1000);
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckNoRtcp2());
  }

  // Test that the mediachannel retains its sending state after the transport
  // becomes non-writable.
  void SendWithWritabilityLoss() {
    CreateChannels(0, 0);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(1U, GetTransport1()->channels().size());
    EXPECT_EQ(1U, GetTransport2()->channels().size());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());

    // Lose writability, which should fail.
    GetTransport1()->SetWritable(false);
    EXPECT_FALSE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckNoRtp2());

    // Regain writability
    GetTransport1()->SetWritable(true);
    EXPECT_TRUE(media_channel1_->sending());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());

    // Lose writability completely
    GetTransport1()->SetDestination(NULL);
    EXPECT_TRUE(media_channel1_->sending());

    // Should fail also.
    EXPECT_FALSE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckNoRtp2());

    // Gain writability back
    GetTransport1()->SetDestination(GetTransport2());
    EXPECT_TRUE(media_channel1_->sending());
    EXPECT_TRUE(SendRtp1());
    EXPECT_TRUE(SendRtp2());
    EXPECT_TRUE(CheckRtp1());
    EXPECT_TRUE(CheckRtp2());
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());
  }

  void SendBundleToBundle(
      const int* pl_types, int len, bool rtcp_mux, bool secure) {
    ASSERT_EQ(2, len);
    int sequence_number1_1 = 0, sequence_number2_2 = 0;
    // Only pl_type1 was added to the bundle filter for both |channel1_|
    // and |channel2_|.
    int pl_type1 = pl_types[0];
    int pl_type2 = pl_types[1];
    int flags = SSRC_MUX | RTCP;
    if (secure) flags |= SECURE;
    uint32_t expected_channels = 2U;
    if (rtcp_mux) {
      flags |= RTCP_MUX;
      expected_channels = 1U;
    }
    CreateChannels(flags, flags);
    EXPECT_TRUE(SendInitiate());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(expected_channels, GetTransport2()->channels().size());
    EXPECT_TRUE(SendAccept());
    EXPECT_EQ(expected_channels, GetTransport1()->channels().size());
    EXPECT_EQ(expected_channels, GetTransport2()->channels().size());
    EXPECT_TRUE(channel1_->bundle_filter()->FindPayloadType(pl_type1));
    EXPECT_TRUE(channel2_->bundle_filter()->FindPayloadType(pl_type1));
    EXPECT_FALSE(channel1_->bundle_filter()->FindPayloadType(pl_type2));
    EXPECT_FALSE(channel2_->bundle_filter()->FindPayloadType(pl_type2));
    // channel1 - should only have media_content2 as remote. i.e. kSsrc2
    EXPECT_TRUE(channel1_->bundle_filter()->FindStream(kSsrc2));
    EXPECT_FALSE(channel1_->bundle_filter()->FindStream(kSsrc1));
    // channel2 - should only have media_content1 as remote. i.e. kSsrc1
    EXPECT_TRUE(channel2_->bundle_filter()->FindStream(kSsrc1));
    EXPECT_FALSE(channel2_->bundle_filter()->FindStream(kSsrc2));

    // Both channels can receive pl_type1 only.
    EXPECT_TRUE(SendCustomRtp1(kSsrc1, ++sequence_number1_1, pl_type1));
    EXPECT_TRUE(CheckCustomRtp2(kSsrc1, sequence_number1_1, pl_type1));
    EXPECT_TRUE(SendCustomRtp2(kSsrc2, ++sequence_number2_2, pl_type1));
    EXPECT_TRUE(CheckCustomRtp1(kSsrc2, sequence_number2_2, pl_type1));
    EXPECT_TRUE(CheckNoRtp1());
    EXPECT_TRUE(CheckNoRtp2());

    // RTCP test
    EXPECT_TRUE(SendCustomRtp1(kSsrc1, ++sequence_number1_1, pl_type2));
    EXPECT_FALSE(CheckCustomRtp2(kSsrc1, sequence_number1_1, pl_type2));
    EXPECT_TRUE(SendCustomRtp2(kSsrc2, ++sequence_number2_2, pl_type2));
    EXPECT_FALSE(CheckCustomRtp1(kSsrc2, sequence_number2_2, pl_type2));

    EXPECT_TRUE(SendCustomRtcp1(kSsrc1));
    EXPECT_TRUE(SendCustomRtcp2(kSsrc2));
    EXPECT_TRUE(CheckCustomRtcp1(kSsrc2));
    EXPECT_TRUE(CheckNoRtcp1());
    EXPECT_TRUE(CheckCustomRtcp2(kSsrc1));
    EXPECT_TRUE(CheckNoRtcp2());

    EXPECT_TRUE(SendCustomRtcp1(kSsrc2));
    EXPECT_TRUE(SendCustomRtcp2(kSsrc1));
    EXPECT_FALSE(CheckCustomRtcp1(kSsrc1));
    EXPECT_FALSE(CheckCustomRtcp2(kSsrc2));
  }

  // Test that the media monitor can be run and gives timely callbacks.
  void TestMediaMonitor() {
    static const int kTimeout = 500;
    CreateChannels(0, 0);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    channel1_->StartMediaMonitor(100);
    channel2_->StartMediaMonitor(100);
    // Ensure we get callbacks and stop.
    EXPECT_TRUE_WAIT(media_info_callbacks1_ > 0, kTimeout);
    EXPECT_TRUE_WAIT(media_info_callbacks2_ > 0, kTimeout);
    channel1_->StopMediaMonitor();
    channel2_->StopMediaMonitor();
    // Ensure a restart of a stopped monitor works.
    channel1_->StartMediaMonitor(100);
    EXPECT_TRUE_WAIT(media_info_callbacks1_ > 0, kTimeout);
    channel1_->StopMediaMonitor();
    // Ensure stopping a stopped monitor is OK.
    channel1_->StopMediaMonitor();
  }

  void TestSetContentFailure() {
    CreateChannels(0, 0);

    auto sdesc = cricket::SessionDescription();
    sdesc.AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP,
                     new cricket::AudioContentDescription());
    sdesc.AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP,
                     new cricket::VideoContentDescription());

    std::string err;
    media_channel1_->set_fail_set_recv_codecs(true);
    EXPECT_FALSE(channel1_->PushdownLocalDescription(
        &sdesc, cricket::CA_OFFER, &err));
    EXPECT_FALSE(channel1_->PushdownLocalDescription(
        &sdesc, cricket::CA_ANSWER, &err));

    media_channel1_->set_fail_set_send_codecs(true);
    EXPECT_FALSE(channel1_->PushdownRemoteDescription(
        &sdesc, cricket::CA_OFFER, &err));
    media_channel1_->set_fail_set_send_codecs(true);
    EXPECT_FALSE(channel1_->PushdownRemoteDescription(
        &sdesc, cricket::CA_ANSWER, &err));
  }

  void TestSendTwoOffers() {
    CreateChannels(0, 0);

    std::string err;
    rtc::scoped_ptr<cricket::SessionDescription> sdesc1(
        CreateSessionDescriptionWithStream(1));
    EXPECT_TRUE(channel1_->PushdownLocalDescription(
        sdesc1.get(), cricket::CA_OFFER, &err));
    EXPECT_TRUE(media_channel1_->HasSendStream(1));

    rtc::scoped_ptr<cricket::SessionDescription> sdesc2(
        CreateSessionDescriptionWithStream(2));
    EXPECT_TRUE(channel1_->PushdownLocalDescription(
        sdesc2.get(), cricket::CA_OFFER, &err));
    EXPECT_FALSE(media_channel1_->HasSendStream(1));
    EXPECT_TRUE(media_channel1_->HasSendStream(2));
  }

  void TestReceiveTwoOffers() {
    CreateChannels(0, 0);

    std::string err;
    rtc::scoped_ptr<cricket::SessionDescription> sdesc1(
        CreateSessionDescriptionWithStream(1));
    EXPECT_TRUE(channel1_->PushdownRemoteDescription(
        sdesc1.get(), cricket::CA_OFFER, &err));
    EXPECT_TRUE(media_channel1_->HasRecvStream(1));

    rtc::scoped_ptr<cricket::SessionDescription> sdesc2(
        CreateSessionDescriptionWithStream(2));
    EXPECT_TRUE(channel1_->PushdownRemoteDescription(
        sdesc2.get(), cricket::CA_OFFER, &err));
    EXPECT_FALSE(media_channel1_->HasRecvStream(1));
    EXPECT_TRUE(media_channel1_->HasRecvStream(2));
  }

  void TestSendPrAnswer() {
    CreateChannels(0, 0);

    std::string err;
    // Receive offer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc1(
        CreateSessionDescriptionWithStream(1));
    EXPECT_TRUE(channel1_->PushdownRemoteDescription(
        sdesc1.get(), cricket::CA_OFFER, &err));
    EXPECT_TRUE(media_channel1_->HasRecvStream(1));

    // Send PR answer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc2(
        CreateSessionDescriptionWithStream(2));
    EXPECT_TRUE(channel1_->PushdownLocalDescription(
        sdesc2.get(), cricket::CA_PRANSWER, &err));
    EXPECT_TRUE(media_channel1_->HasRecvStream(1));
    EXPECT_TRUE(media_channel1_->HasSendStream(2));

    // Send answer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc3(
        CreateSessionDescriptionWithStream(3));
    EXPECT_TRUE(channel1_->PushdownLocalDescription(
        sdesc3.get(), cricket::CA_ANSWER, &err));
    EXPECT_TRUE(media_channel1_->HasRecvStream(1));
    EXPECT_FALSE(media_channel1_->HasSendStream(2));
    EXPECT_TRUE(media_channel1_->HasSendStream(3));
  }

  void TestReceivePrAnswer() {
    CreateChannels(0, 0);

    std::string err;
    // Send offer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc1(
        CreateSessionDescriptionWithStream(1));
    EXPECT_TRUE(channel1_->PushdownLocalDescription(
        sdesc1.get(), cricket::CA_OFFER, &err));
    EXPECT_TRUE(media_channel1_->HasSendStream(1));

    // Receive PR answer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc2(
        CreateSessionDescriptionWithStream(2));
    EXPECT_TRUE(channel1_->PushdownRemoteDescription(
        sdesc2.get(), cricket::CA_PRANSWER, &err));
    EXPECT_TRUE(media_channel1_->HasSendStream(1));
    EXPECT_TRUE(media_channel1_->HasRecvStream(2));

    // Receive answer
    rtc::scoped_ptr<cricket::SessionDescription> sdesc3(
        CreateSessionDescriptionWithStream(3));
    EXPECT_TRUE(channel1_->PushdownRemoteDescription(
        sdesc3.get(), cricket::CA_ANSWER, &err));
    EXPECT_TRUE(media_channel1_->HasSendStream(1));
    EXPECT_FALSE(media_channel1_->HasRecvStream(2));
    EXPECT_TRUE(media_channel1_->HasRecvStream(3));
  }

  void TestFlushRtcp() {
    bool send_rtcp1;

    CreateChannels(RTCP, RTCP);
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    ASSERT_TRUE(GetTransport1());
    ASSERT_TRUE(GetTransport2());
    EXPECT_EQ(2U, GetTransport1()->channels().size());
    EXPECT_EQ(2U, GetTransport2()->channels().size());

    // Send RTCP1 from a different thread.
    CallOnThreadAndWaitForDone(&ChannelTest<T>::SendRtcp1, &send_rtcp1);
    EXPECT_TRUE(send_rtcp1);
    // The sending message is only posted.  channel2_ should be empty.
    EXPECT_TRUE(CheckNoRtcp2());

    // When channel1_ is deleted, the RTCP packet should be sent out to
    // channel2_.
    channel1_.reset();
    EXPECT_TRUE(CheckRtcp2());
  }

  void TestSrtpError(int pl_type) {
    struct SrtpErrorHandler : public sigslot::has_slots<> {
      SrtpErrorHandler() :
          mode_(cricket::SrtpFilter::UNPROTECT),
          error_(cricket::SrtpFilter::ERROR_NONE) {}
      void OnSrtpError(uint32 ssrc, cricket::SrtpFilter::Mode mode,
                       cricket::SrtpFilter::Error error) {
        mode_ = mode;
        error_ = error;
      }
      cricket::SrtpFilter::Mode mode_;
      cricket::SrtpFilter::Error error_;
    } error_handler;

    // For Audio, only pl_type 0 is added to the bundle filter.
    // For Video, only pl_type 97 is added to the bundle filter.
    // So we need to pass in pl_type so that the packet can pass through
    // the bundle filter before it can be processed by the srtp filter.
    // The packet is not a valid srtp packet because it is too short.
    unsigned const char kBadPacket[] = {0x84,
                                        static_cast<unsigned char>(pl_type),
                                        0x00,
                                        0x01,
                                        0x00,
                                        0x00,
                                        0x00,
                                        0x00,
                                        0x00,
                                        0x00,
                                        0x00,
                                        0x01};
    CreateChannels(RTCP | SECURE, RTCP | SECURE);
    EXPECT_FALSE(channel1_->secure());
    EXPECT_FALSE(channel2_->secure());
    EXPECT_TRUE(SendInitiate());
    EXPECT_TRUE(SendAccept());
    EXPECT_TRUE(channel1_->secure());
    EXPECT_TRUE(channel2_->secure());
    channel2_->srtp_filter()->set_signal_silent_time(250);
    channel2_->srtp_filter()->SignalSrtpError.connect(
        &error_handler, &SrtpErrorHandler::OnSrtpError);

    // Testing failures in sending packets.
    EXPECT_FALSE(media_channel2_->SendRtp(kBadPacket, sizeof(kBadPacket),
                                          rtc::PacketOptions()));
    // The first failure will trigger an error.
    EXPECT_EQ_WAIT(cricket::SrtpFilter::ERROR_FAIL, error_handler.error_, 500);
    EXPECT_EQ(cricket::SrtpFilter::PROTECT, error_handler.mode_);
    error_handler.error_ = cricket::SrtpFilter::ERROR_NONE;
    error_handler.mode_ = cricket::SrtpFilter::UNPROTECT;
    // The next 250 ms failures will not trigger an error.
    EXPECT_FALSE(media_channel2_->SendRtp(kBadPacket, sizeof(kBadPacket),
                                          rtc::PacketOptions()));
    // Wait for a while to ensure no message comes in.
    rtc::Thread::Current()->ProcessMessages(200);
    EXPECT_EQ(cricket::SrtpFilter::ERROR_NONE, error_handler.error_);
    EXPECT_EQ(cricket::SrtpFilter::UNPROTECT, error_handler.mode_);
    // Wait for a little more - the error will be triggered again.
    rtc::Thread::Current()->ProcessMessages(200);
    EXPECT_FALSE(media_channel2_->SendRtp(kBadPacket, sizeof(kBadPacket),
                                          rtc::PacketOptions()));
    EXPECT_EQ_WAIT(cricket::SrtpFilter::ERROR_FAIL, error_handler.error_, 500);
    EXPECT_EQ(cricket::SrtpFilter::PROTECT, error_handler.mode_);

    // Testing failures in receiving packets.
    error_handler.error_ = cricket::SrtpFilter::ERROR_NONE;
    error_handler.mode_ = cricket::SrtpFilter::UNPROTECT;

    cricket::TransportChannel* transport_channel =
        channel2_->transport_channel();
    transport_channel->SignalReadPacket(
        transport_channel, reinterpret_cast<const char*>(kBadPacket),
        sizeof(kBadPacket), rtc::PacketTime(), 0);
    EXPECT_EQ_WAIT(cricket::SrtpFilter::ERROR_FAIL, error_handler.error_, 500);
    EXPECT_EQ(cricket::SrtpFilter::UNPROTECT, error_handler.mode_);
  }

  void TestOnReadyToSend() {
    CreateChannels(RTCP, RTCP);
    TransportChannel* rtp = channel1_->transport_channel();
    TransportChannel* rtcp = channel1_->rtcp_transport_channel();
    EXPECT_FALSE(media_channel1_->ready_to_send());
    rtp->SignalReadyToSend(rtp);
    EXPECT_FALSE(media_channel1_->ready_to_send());
    rtcp->SignalReadyToSend(rtcp);
    // MediaChannel::OnReadyToSend only be called when both rtp and rtcp
    // channel are ready to send.
    EXPECT_TRUE(media_channel1_->ready_to_send());

    // rtp channel becomes not ready to send will be propagated to mediachannel
    channel1_->SetReadyToSend(false, false);
    EXPECT_FALSE(media_channel1_->ready_to_send());
    channel1_->SetReadyToSend(false, true);
    EXPECT_TRUE(media_channel1_->ready_to_send());

    // rtcp channel becomes not ready to send will be propagated to mediachannel
    channel1_->SetReadyToSend(true, false);
    EXPECT_FALSE(media_channel1_->ready_to_send());
    channel1_->SetReadyToSend(true, true);
    EXPECT_TRUE(media_channel1_->ready_to_send());
  }

  void TestOnReadyToSendWithRtcpMux() {
    CreateChannels(RTCP, RTCP);
    typename T::Content content;
    CreateContent(0, kPcmuCodec, kH264Codec, &content);
    // Both sides agree on mux. Should no longer be a separate RTCP channel.
    content.set_rtcp_mux(true);
    EXPECT_TRUE(channel1_->SetLocalContent(&content, CA_OFFER, NULL));
    EXPECT_TRUE(channel1_->SetRemoteContent(&content, CA_ANSWER, NULL));
    EXPECT_TRUE(channel1_->rtcp_transport_channel() == NULL);
    TransportChannel* rtp = channel1_->transport_channel();
    EXPECT_FALSE(media_channel1_->ready_to_send());
    // In the case of rtcp mux, the SignalReadyToSend() from rtp channel
    // should trigger the MediaChannel's OnReadyToSend.
    rtp->SignalReadyToSend(rtp);
    EXPECT_TRUE(media_channel1_->ready_to_send());
    channel1_->SetReadyToSend(false, false);
    EXPECT_FALSE(media_channel1_->ready_to_send());
  }

 protected:
  // TODO(pbos): Remove playout from all media channels and let renderers mute
  // themselves.
  const bool verify_playout_;
  cricket::FakeTransportController transport_controller1_;
  cricket::FakeTransportController transport_controller2_;
  cricket::FakeMediaEngine media_engine_;
  // The media channels are owned by the voice channel objects below.
  typename T::MediaChannel* media_channel1_;
  typename T::MediaChannel* media_channel2_;
  rtc::scoped_ptr<typename T::Channel> channel1_;
  rtc::scoped_ptr<typename T::Channel> channel2_;
  typename T::Content local_media_content1_;
  typename T::Content local_media_content2_;
  typename T::Content remote_media_content1_;
  typename T::Content remote_media_content2_;
  // The RTP and RTCP packets to send in the tests.
  std::string rtp_packet_;
  std::string rtcp_packet_;
  int media_info_callbacks1_;
  int media_info_callbacks2_;
};

template<>
void ChannelTest<VoiceTraits>::CreateContent(
    int flags,
    const cricket::AudioCodec& audio_codec,
    const cricket::VideoCodec& video_codec,
    cricket::AudioContentDescription* audio) {
  audio->AddCodec(audio_codec);
  audio->set_rtcp_mux((flags & RTCP_MUX) != 0);
  if (flags & SECURE) {
    audio->AddCrypto(cricket::CryptoParams(
        1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
        "inline:" + rtc::CreateRandomString(40), std::string()));
  }
}

template<>
void ChannelTest<VoiceTraits>::CopyContent(
    const cricket::AudioContentDescription& source,
    cricket::AudioContentDescription* audio) {
  *audio = source;
}

template<>
bool ChannelTest<VoiceTraits>::CodecMatches(const cricket::AudioCodec& c1,
                                            const cricket::AudioCodec& c2) {
  return c1.name == c2.name && c1.clockrate == c2.clockrate &&
      c1.bitrate == c2.bitrate && c1.channels == c2.channels;
}

template <>
void ChannelTest<VoiceTraits>::AddLegacyStreamInContent(
    uint32_t ssrc,
    int flags,
    cricket::AudioContentDescription* audio) {
  audio->AddLegacyStream(ssrc);
}

class VoiceChannelTest
    : public ChannelTest<VoiceTraits> {
 public:
  typedef ChannelTest<VoiceTraits> Base;
  VoiceChannelTest()
      : Base(true,
             kPcmuFrame,
             sizeof(kPcmuFrame),
             kRtcpReport,
             sizeof(kRtcpReport)) {}
};

// override to add NULL parameter
template <>
cricket::VideoChannel* ChannelTest<VideoTraits>::CreateChannel(
    rtc::Thread* thread,
    cricket::MediaEngineInterface* engine,
    cricket::FakeVideoMediaChannel* ch,
    cricket::TransportController* transport_controller,
    bool rtcp) {
  cricket::VideoChannel* channel = new cricket::VideoChannel(
      thread, ch, transport_controller, cricket::CN_VIDEO, rtcp);
  if (!channel->Init()) {
    delete channel;
    channel = NULL;
  }
  return channel;
}

// override to add 0 parameter
template<>
bool ChannelTest<VideoTraits>::AddStream1(int id) {
  return channel1_->AddRecvStream(cricket::StreamParams::CreateLegacy(id));
}

template<>
void ChannelTest<VideoTraits>::CreateContent(
    int flags,
    const cricket::AudioCodec& audio_codec,
    const cricket::VideoCodec& video_codec,
    cricket::VideoContentDescription* video) {
  video->AddCodec(video_codec);
  video->set_rtcp_mux((flags & RTCP_MUX) != 0);
  if (flags & SECURE) {
    video->AddCrypto(cricket::CryptoParams(
        1, rtc::CS_AES_CM_128_HMAC_SHA1_80,
        "inline:" + rtc::CreateRandomString(40), std::string()));
  }
}

template<>
void ChannelTest<VideoTraits>::CopyContent(
    const cricket::VideoContentDescription& source,
    cricket::VideoContentDescription* video) {
  *video = source;
}

template<>
bool ChannelTest<VideoTraits>::CodecMatches(const cricket::VideoCodec& c1,
                                            const cricket::VideoCodec& c2) {
  return c1.name == c2.name && c1.width == c2.width && c1.height == c2.height &&
      c1.framerate == c2.framerate;
}

template <>
void ChannelTest<VideoTraits>::AddLegacyStreamInContent(
    uint32_t ssrc,
    int flags,
    cricket::VideoContentDescription* video) {
  video->AddLegacyStream(ssrc);
}

class VideoChannelTest
    : public ChannelTest<VideoTraits> {
 public:
  typedef ChannelTest<VideoTraits> Base;
  VideoChannelTest()
      : Base(false,
             kH264Packet,
             sizeof(kH264Packet),
             kRtcpReport,
             sizeof(kRtcpReport)) {}
};


// VoiceChannelTest

TEST_F(VoiceChannelTest, TestInit) {
  Base::TestInit();
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));
  EXPECT_TRUE(media_channel1_->dtmf_info_queue().empty());
}

TEST_F(VoiceChannelTest, TestSetContents) {
  Base::TestSetContents();
}

TEST_F(VoiceChannelTest, TestSetContentsNullOffer) {
  Base::TestSetContentsNullOffer();
}

TEST_F(VoiceChannelTest, TestSetContentsRtcpMux) {
  Base::TestSetContentsRtcpMux();
}

TEST_F(VoiceChannelTest, TestSetContentsRtcpMuxWithPrAnswer) {
  Base::TestSetContentsRtcpMux();
}

TEST_F(VoiceChannelTest, TestSetRemoteContentUpdate) {
  Base::TestSetRemoteContentUpdate();
}

TEST_F(VoiceChannelTest, TestStreams) {
  Base::TestStreams();
}

TEST_F(VoiceChannelTest, TestUpdateStreamsInLocalContent) {
  Base::TestUpdateStreamsInLocalContent();
}

TEST_F(VoiceChannelTest, TestUpdateRemoteStreamsInContent) {
  Base::TestUpdateStreamsInRemoteContent();
}

TEST_F(VoiceChannelTest, TestChangeStreamParamsInContent) {
  Base::TestChangeStreamParamsInContent();
}

TEST_F(VoiceChannelTest, TestPlayoutAndSendingStates) {
  Base::TestPlayoutAndSendingStates();
}

TEST_F(VoiceChannelTest, TestMuteStream) {
  CreateChannels(0, 0);
  // Test that we can Mute the default channel even though the sending SSRC
  // is unknown.
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));
  EXPECT_TRUE(channel1_->SetAudioSend(0, false, nullptr, nullptr));
  EXPECT_TRUE(media_channel1_->IsStreamMuted(0));
  EXPECT_TRUE(channel1_->SetAudioSend(0, true, nullptr, nullptr));
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));

  // Test that we can not mute an unknown SSRC.
  EXPECT_FALSE(channel1_->SetAudioSend(kSsrc1, false, nullptr, nullptr));

  SendInitiate();
  // After the local session description has been set, we can mute a stream
  // with its SSRC.
  EXPECT_TRUE(channel1_->SetAudioSend(kSsrc1, false, nullptr, nullptr));
  EXPECT_TRUE(media_channel1_->IsStreamMuted(kSsrc1));
  EXPECT_TRUE(channel1_->SetAudioSend(kSsrc1, true, nullptr, nullptr));
  EXPECT_FALSE(media_channel1_->IsStreamMuted(kSsrc1));
}

TEST_F(VoiceChannelTest, TestMediaContentDirection) {
  Base::TestMediaContentDirection();
}

TEST_F(VoiceChannelTest, TestCallSetup) {
  Base::TestCallSetup();
}

TEST_F(VoiceChannelTest, TestCallTeardownRtcpMux) {
  Base::TestCallTeardownRtcpMux();
}

TEST_F(VoiceChannelTest, SendRtpToRtp) {
  Base::SendRtpToRtp();
}

TEST_F(VoiceChannelTest, SendNoRtcpToNoRtcp) {
  Base::SendNoRtcpToNoRtcp();
}

TEST_F(VoiceChannelTest, SendNoRtcpToRtcp) {
  Base::SendNoRtcpToRtcp();
}

TEST_F(VoiceChannelTest, SendRtcpToNoRtcp) {
  Base::SendRtcpToNoRtcp();
}

TEST_F(VoiceChannelTest, SendRtcpToRtcp) {
  Base::SendRtcpToRtcp();
}

TEST_F(VoiceChannelTest, SendRtcpMuxToRtcp) {
  Base::SendRtcpMuxToRtcp();
}

TEST_F(VoiceChannelTest, SendRtcpMuxToRtcpMux) {
  Base::SendRtcpMuxToRtcpMux();
}

TEST_F(VoiceChannelTest, SendRequireRtcpMuxToRtcpMux) {
  Base::SendRequireRtcpMuxToRtcpMux();
}

TEST_F(VoiceChannelTest, SendRtcpMuxToRequireRtcpMux) {
  Base::SendRtcpMuxToRequireRtcpMux();
}

TEST_F(VoiceChannelTest, SendRequireRtcpMuxToRequireRtcpMux) {
  Base::SendRequireRtcpMuxToRequireRtcpMux();
}

TEST_F(VoiceChannelTest, SendRequireRtcpMuxToNoRtcpMux) {
  Base::SendRequireRtcpMuxToNoRtcpMux();
}

TEST_F(VoiceChannelTest, SendEarlyRtcpMuxToRtcp) {
  Base::SendEarlyRtcpMuxToRtcp();
}

TEST_F(VoiceChannelTest, SendEarlyRtcpMuxToRtcpMux) {
  Base::SendEarlyRtcpMuxToRtcpMux();
}

TEST_F(VoiceChannelTest, SendSrtpToSrtpRtcpMux) {
  Base::SendSrtpToSrtp(RTCP_MUX, RTCP_MUX);
}

TEST_F(VoiceChannelTest, SendSrtpToRtp) {
  Base::SendSrtpToSrtp();
}

TEST_F(VoiceChannelTest, SendSrtcpMux) {
  Base::SendSrtpToSrtp(RTCP_MUX, RTCP_MUX);
}

TEST_F(VoiceChannelTest, SendDtlsSrtpToSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS, 0);
}

TEST_F(VoiceChannelTest, SendDtlsSrtpToDtlsSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS, DTLS);
}

TEST_F(VoiceChannelTest, SendDtlsSrtpToDtlsSrtpRtcpMux) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS | RTCP_MUX, DTLS | RTCP_MUX);
}

TEST_F(VoiceChannelTest, SendEarlyMediaUsingRtcpMuxSrtp) {
  Base::SendEarlyMediaUsingRtcpMuxSrtp();
}

TEST_F(VoiceChannelTest, SendRtpToRtpOnThread) {
  Base::SendRtpToRtpOnThread();
}

TEST_F(VoiceChannelTest, SendSrtpToSrtpOnThread) {
  Base::SendSrtpToSrtpOnThread();
}

TEST_F(VoiceChannelTest, SendWithWritabilityLoss) {
  Base::SendWithWritabilityLoss();
}

TEST_F(VoiceChannelTest, TestMediaMonitor) {
  Base::TestMediaMonitor();
}

// Test that PressDTMF properly forwards to the media channel.
TEST_F(VoiceChannelTest, TestDtmf) {
  CreateChannels(0, 0);
  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());
  EXPECT_EQ(0U, media_channel1_->dtmf_info_queue().size());

  EXPECT_TRUE(channel1_->PressDTMF(1, true));
  EXPECT_TRUE(channel1_->PressDTMF(8, false));

  ASSERT_EQ(2U, media_channel1_->dtmf_info_queue().size());
  EXPECT_TRUE(CompareDtmfInfo(media_channel1_->dtmf_info_queue()[0],
                              0, 1, 160, cricket::DF_PLAY | cricket::DF_SEND));
  EXPECT_TRUE(CompareDtmfInfo(media_channel1_->dtmf_info_queue()[1],
                              0, 8, 160, cricket::DF_SEND));
}

// Test that InsertDtmf properly forwards to the media channel.
TEST_F(VoiceChannelTest, TestInsertDtmf) {
  CreateChannels(0, 0);
  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());
  EXPECT_EQ(0U, media_channel1_->dtmf_info_queue().size());

  EXPECT_TRUE(channel1_->InsertDtmf(1, 3, 100, cricket::DF_SEND));
  EXPECT_TRUE(channel1_->InsertDtmf(2, 5, 110, cricket::DF_PLAY));
  EXPECT_TRUE(channel1_->InsertDtmf(3, 7, 120,
                                    cricket::DF_PLAY | cricket::DF_SEND));

  ASSERT_EQ(3U, media_channel1_->dtmf_info_queue().size());
  EXPECT_TRUE(CompareDtmfInfo(media_channel1_->dtmf_info_queue()[0],
                              1, 3, 100, cricket::DF_SEND));
  EXPECT_TRUE(CompareDtmfInfo(media_channel1_->dtmf_info_queue()[1],
                              2, 5, 110, cricket::DF_PLAY));
  EXPECT_TRUE(CompareDtmfInfo(media_channel1_->dtmf_info_queue()[2],
                              3, 7, 120, cricket::DF_PLAY | cricket::DF_SEND));
}

TEST_F(VoiceChannelTest, TestSetContentFailure) {
  Base::TestSetContentFailure();
}

TEST_F(VoiceChannelTest, TestSendTwoOffers) {
  Base::TestSendTwoOffers();
}

TEST_F(VoiceChannelTest, TestReceiveTwoOffers) {
  Base::TestReceiveTwoOffers();
}

TEST_F(VoiceChannelTest, TestSendPrAnswer) {
  Base::TestSendPrAnswer();
}

TEST_F(VoiceChannelTest, TestReceivePrAnswer) {
  Base::TestReceivePrAnswer();
}

TEST_F(VoiceChannelTest, TestFlushRtcp) {
  Base::TestFlushRtcp();
}

TEST_F(VoiceChannelTest, TestSrtpError) {
  Base::TestSrtpError(kAudioPts[0]);
}

TEST_F(VoiceChannelTest, TestOnReadyToSend) {
  Base::TestOnReadyToSend();
}

TEST_F(VoiceChannelTest, TestOnReadyToSendWithRtcpMux) {
  Base::TestOnReadyToSendWithRtcpMux();
}

// Test that we can scale the output volume properly for 1:1 calls.
TEST_F(VoiceChannelTest, TestScaleVolume1to1Call) {
  CreateChannels(RTCP, RTCP);
  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());
  double volume;

  // Default is (1.0).
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);
  // invalid ssrc.
  EXPECT_FALSE(media_channel1_->GetOutputVolume(3, &volume));

  // Set scale to (1.5).
  EXPECT_TRUE(channel1_->SetOutputVolume(0, 1.5));
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(1.5, volume);

  // Set scale to (0).
  EXPECT_TRUE(channel1_->SetOutputVolume(0, 0.0));
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(0.0, volume);
}

// Test that we can scale the output volume properly for multiway calls.
TEST_F(VoiceChannelTest, TestScaleVolumeMultiwayCall) {
  CreateChannels(RTCP, RTCP);
  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());
  EXPECT_TRUE(AddStream1(1));
  EXPECT_TRUE(AddStream1(2));

  double volume;
  // Default is (1.0).
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(1, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(2, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);
  // invalid ssrc.
  EXPECT_FALSE(media_channel1_->GetOutputVolume(3, &volume));

  // Set scale to (1.5) for ssrc = 1.
  EXPECT_TRUE(channel1_->SetOutputVolume(1, 1.5));
  EXPECT_TRUE(media_channel1_->GetOutputVolume(1, &volume));
  EXPECT_DOUBLE_EQ(1.5, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(2, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(1.0, volume);

  // Set scale to (0) for all ssrcs.
  EXPECT_TRUE(channel1_->SetOutputVolume(0,  0.0));
  EXPECT_TRUE(media_channel1_->GetOutputVolume(0, &volume));
  EXPECT_DOUBLE_EQ(0.0, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(1, &volume));
  EXPECT_DOUBLE_EQ(0.0, volume);
  EXPECT_TRUE(media_channel1_->GetOutputVolume(2, &volume));
  EXPECT_DOUBLE_EQ(0.0, volume);
}

TEST_F(VoiceChannelTest, SendBundleToBundle) {
  Base::SendBundleToBundle(kAudioPts, ARRAY_SIZE(kAudioPts), false, false);
}

TEST_F(VoiceChannelTest, SendBundleToBundleSecure) {
  Base::SendBundleToBundle(kAudioPts, ARRAY_SIZE(kAudioPts), false, true);
}

TEST_F(VoiceChannelTest, SendBundleToBundleWithRtcpMux) {
  Base::SendBundleToBundle(
      kAudioPts, ARRAY_SIZE(kAudioPts), true, false);
}

TEST_F(VoiceChannelTest, SendBundleToBundleWithRtcpMuxSecure) {
  Base::SendBundleToBundle(
      kAudioPts, ARRAY_SIZE(kAudioPts), true, true);
}

// VideoChannelTest
TEST_F(VideoChannelTest, TestInit) {
  Base::TestInit();
}

TEST_F(VideoChannelTest, TestSetContents) {
  Base::TestSetContents();
}

TEST_F(VideoChannelTest, TestSetContentsNullOffer) {
  Base::TestSetContentsNullOffer();
}

TEST_F(VideoChannelTest, TestSetContentsRtcpMux) {
  Base::TestSetContentsRtcpMux();
}

TEST_F(VideoChannelTest, TestSetContentsRtcpMuxWithPrAnswer) {
  Base::TestSetContentsRtcpMux();
}

TEST_F(VideoChannelTest, TestSetRemoteContentUpdate) {
  Base::TestSetRemoteContentUpdate();
}

TEST_F(VideoChannelTest, TestStreams) {
  Base::TestStreams();
}

TEST_F(VideoChannelTest, TestScreencastEvents) {
  const int kTimeoutMs = 500;
  TestInit();
  cricket::ScreencastEventCatcher catcher;
  channel1_->SignalScreencastWindowEvent.connect(
      &catcher,
      &cricket::ScreencastEventCatcher::OnEvent);

  rtc::scoped_ptr<cricket::FakeScreenCapturerFactory>
      screen_capturer_factory(new cricket::FakeScreenCapturerFactory());
  cricket::VideoCapturer* screen_capturer = screen_capturer_factory->Create(
      ScreencastId(WindowId(0)));
  ASSERT_TRUE(screen_capturer != NULL);

  EXPECT_TRUE(channel1_->AddScreencast(0, screen_capturer));
  EXPECT_EQ_WAIT(cricket::CS_STOPPED, screen_capturer_factory->capture_state(),
                 kTimeoutMs);

  screen_capturer->SignalStateChange(screen_capturer, cricket::CS_PAUSED);
  EXPECT_EQ_WAIT(rtc::WE_MINIMIZE, catcher.event(), kTimeoutMs);

  screen_capturer->SignalStateChange(screen_capturer, cricket::CS_RUNNING);
  EXPECT_EQ_WAIT(rtc::WE_RESTORE, catcher.event(), kTimeoutMs);

  screen_capturer->SignalStateChange(screen_capturer, cricket::CS_STOPPED);
  EXPECT_EQ_WAIT(rtc::WE_CLOSE, catcher.event(), kTimeoutMs);

  EXPECT_TRUE(channel1_->RemoveScreencast(0));
}

TEST_F(VideoChannelTest, TestUpdateStreamsInLocalContent) {
  Base::TestUpdateStreamsInLocalContent();
}

TEST_F(VideoChannelTest, TestUpdateRemoteStreamsInContent) {
  Base::TestUpdateStreamsInRemoteContent();
}

TEST_F(VideoChannelTest, TestChangeStreamParamsInContent) {
  Base::TestChangeStreamParamsInContent();
}

TEST_F(VideoChannelTest, TestPlayoutAndSendingStates) {
  Base::TestPlayoutAndSendingStates();
}

TEST_F(VideoChannelTest, TestMuteStream) {
  CreateChannels(0, 0);
  // Test that we can Mute the default channel even though the sending SSRC
  // is unknown.
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));
  EXPECT_TRUE(channel1_->SetVideoSend(0, false, nullptr));
  EXPECT_TRUE(media_channel1_->IsStreamMuted(0));
  EXPECT_TRUE(channel1_->SetVideoSend(0, true, nullptr));
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));
  // Test that we can not mute an unknown SSRC.
  EXPECT_FALSE(channel1_->SetVideoSend(kSsrc1, false, nullptr));
  SendInitiate();
  // After the local session description has been set, we can mute a stream
  // with its SSRC.
  EXPECT_TRUE(channel1_->SetVideoSend(kSsrc1, false, nullptr));
  EXPECT_TRUE(media_channel1_->IsStreamMuted(kSsrc1));
  EXPECT_TRUE(channel1_->SetVideoSend(kSsrc1, true, nullptr));
  EXPECT_FALSE(media_channel1_->IsStreamMuted(kSsrc1));
}

TEST_F(VideoChannelTest, TestMediaContentDirection) {
  Base::TestMediaContentDirection();
}

TEST_F(VideoChannelTest, TestCallSetup) {
  Base::TestCallSetup();
}

TEST_F(VideoChannelTest, TestCallTeardownRtcpMux) {
  Base::TestCallTeardownRtcpMux();
}

TEST_F(VideoChannelTest, SendRtpToRtp) {
  Base::SendRtpToRtp();
}

TEST_F(VideoChannelTest, SendNoRtcpToNoRtcp) {
  Base::SendNoRtcpToNoRtcp();
}

TEST_F(VideoChannelTest, SendNoRtcpToRtcp) {
  Base::SendNoRtcpToRtcp();
}

TEST_F(VideoChannelTest, SendRtcpToNoRtcp) {
  Base::SendRtcpToNoRtcp();
}

TEST_F(VideoChannelTest, SendRtcpToRtcp) {
  Base::SendRtcpToRtcp();
}

TEST_F(VideoChannelTest, SendRtcpMuxToRtcp) {
  Base::SendRtcpMuxToRtcp();
}

TEST_F(VideoChannelTest, SendRtcpMuxToRtcpMux) {
  Base::SendRtcpMuxToRtcpMux();
}

TEST_F(VideoChannelTest, SendRequireRtcpMuxToRtcpMux) {
  Base::SendRequireRtcpMuxToRtcpMux();
}

TEST_F(VideoChannelTest, SendRtcpMuxToRequireRtcpMux) {
  Base::SendRtcpMuxToRequireRtcpMux();
}

TEST_F(VideoChannelTest, SendRequireRtcpMuxToRequireRtcpMux) {
  Base::SendRequireRtcpMuxToRequireRtcpMux();
}

TEST_F(VideoChannelTest, SendRequireRtcpMuxToNoRtcpMux) {
  Base::SendRequireRtcpMuxToNoRtcpMux();
}

TEST_F(VideoChannelTest, SendEarlyRtcpMuxToRtcp) {
  Base::SendEarlyRtcpMuxToRtcp();
}

TEST_F(VideoChannelTest, SendEarlyRtcpMuxToRtcpMux) {
  Base::SendEarlyRtcpMuxToRtcpMux();
}

TEST_F(VideoChannelTest, SendSrtpToSrtp) {
  Base::SendSrtpToSrtp();
}

TEST_F(VideoChannelTest, SendSrtpToRtp) {
  Base::SendSrtpToSrtp();
}

TEST_F(VideoChannelTest, SendDtlsSrtpToSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS, 0);
}

TEST_F(VideoChannelTest, SendDtlsSrtpToDtlsSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS, DTLS);
}

TEST_F(VideoChannelTest, SendDtlsSrtpToDtlsSrtpRtcpMux) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  Base::SendSrtpToSrtp(DTLS | RTCP_MUX, DTLS | RTCP_MUX);
}

TEST_F(VideoChannelTest, SendSrtcpMux) {
  Base::SendSrtpToSrtp(RTCP_MUX, RTCP_MUX);
}

TEST_F(VideoChannelTest, SendEarlyMediaUsingRtcpMuxSrtp) {
  Base::SendEarlyMediaUsingRtcpMuxSrtp();
}

TEST_F(VideoChannelTest, SendRtpToRtpOnThread) {
  Base::SendRtpToRtpOnThread();
}

TEST_F(VideoChannelTest, SendSrtpToSrtpOnThread) {
  Base::SendSrtpToSrtpOnThread();
}

TEST_F(VideoChannelTest, SendWithWritabilityLoss) {
  Base::SendWithWritabilityLoss();
}

TEST_F(VideoChannelTest, TestMediaMonitor) {
  Base::TestMediaMonitor();
}

TEST_F(VideoChannelTest, TestSetContentFailure) {
  Base::TestSetContentFailure();
}

TEST_F(VideoChannelTest, TestSendTwoOffers) {
  Base::TestSendTwoOffers();
}

TEST_F(VideoChannelTest, TestReceiveTwoOffers) {
  Base::TestReceiveTwoOffers();
}

TEST_F(VideoChannelTest, TestSendPrAnswer) {
  Base::TestSendPrAnswer();
}

TEST_F(VideoChannelTest, TestReceivePrAnswer) {
  Base::TestReceivePrAnswer();
}

TEST_F(VideoChannelTest, TestFlushRtcp) {
  Base::TestFlushRtcp();
}

TEST_F(VideoChannelTest, SendBundleToBundle) {
  Base::SendBundleToBundle(kVideoPts, ARRAY_SIZE(kVideoPts), false, false);
}

TEST_F(VideoChannelTest, SendBundleToBundleSecure) {
  Base::SendBundleToBundle(kVideoPts, ARRAY_SIZE(kVideoPts), false, true);
}

TEST_F(VideoChannelTest, SendBundleToBundleWithRtcpMux) {
  Base::SendBundleToBundle(
      kVideoPts, ARRAY_SIZE(kVideoPts), true, false);
}

TEST_F(VideoChannelTest, SendBundleToBundleWithRtcpMuxSecure) {
  Base::SendBundleToBundle(
      kVideoPts, ARRAY_SIZE(kVideoPts), true, true);
}

TEST_F(VideoChannelTest, TestSrtpError) {
  Base::TestSrtpError(kVideoPts[0]);
}

TEST_F(VideoChannelTest, TestOnReadyToSend) {
  Base::TestOnReadyToSend();
}

TEST_F(VideoChannelTest, TestOnReadyToSendWithRtcpMux) {
  Base::TestOnReadyToSendWithRtcpMux();
}

TEST_F(VideoChannelTest, TestApplyViewRequest) {
  CreateChannels(0, 0);
  cricket::StreamParams stream2;
  stream2.id = "stream2";
  stream2.ssrcs.push_back(2222);
  local_media_content1_.AddStream(stream2);

  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());

  cricket::VideoFormat send_format;
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(kSsrc1, &send_format));
  EXPECT_EQ(640, send_format.width);
  EXPECT_EQ(400, send_format.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), send_format.interval);

  cricket::ViewRequest request;
  // stream1: 320x200x15; stream2: 0x0x0
  request.static_video_views.push_back(cricket::StaticVideoView(
      cricket::StreamSelector(kSsrc1), 320, 200, 15));
  EXPECT_TRUE(channel1_->ApplyViewRequest(request));
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(kSsrc1, &send_format));
  EXPECT_EQ(320, send_format.width);
  EXPECT_EQ(200, send_format.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), send_format.interval);
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(2222, &send_format));
  EXPECT_EQ(0, send_format.width);
  EXPECT_EQ(0, send_format.height);

  // stream1: 160x100x8; stream2: 0x0x0
  request.static_video_views.clear();
  request.static_video_views.push_back(cricket::StaticVideoView(
      cricket::StreamSelector(kSsrc1), 160, 100, 8));
  EXPECT_TRUE(channel1_->ApplyViewRequest(request));
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(kSsrc1, &send_format));
  EXPECT_EQ(160, send_format.width);
  EXPECT_EQ(100, send_format.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(8), send_format.interval);

  // stream1: 0x0x0; stream2: 640x400x30
  request.static_video_views.clear();
  request.static_video_views.push_back(cricket::StaticVideoView(
      cricket::StreamSelector(std::string(), stream2.id), 640, 400, 30));
  EXPECT_TRUE(channel1_->ApplyViewRequest(request));
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(kSsrc1, &send_format));
  EXPECT_EQ(0, send_format.width);
  EXPECT_EQ(0, send_format.height);
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(2222, &send_format));
  EXPECT_EQ(640, send_format.width);
  EXPECT_EQ(400, send_format.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), send_format.interval);

  // stream1: 0x0x0; stream2: 0x0x0
  request.static_video_views.clear();
  EXPECT_TRUE(channel1_->ApplyViewRequest(request));
  EXPECT_TRUE(media_channel1_->GetSendStreamFormat(kSsrc1, &send_format));
  EXPECT_EQ(0, send_format.width);
  EXPECT_EQ(0, send_format.height);
}


// DataChannelTest

class DataChannelTest
    : public ChannelTest<DataTraits> {
 public:
  typedef ChannelTest<DataTraits>
  Base;
  DataChannelTest()
      : Base(true,
             kDataPacket,
             sizeof(kDataPacket),
             kRtcpReport,
             sizeof(kRtcpReport)) {}
};

// Override to avoid engine channel parameter.
template <>
cricket::DataChannel* ChannelTest<DataTraits>::CreateChannel(
    rtc::Thread* thread,
    cricket::MediaEngineInterface* engine,
    cricket::FakeDataMediaChannel* ch,
    cricket::TransportController* transport_controller,
    bool rtcp) {
  cricket::DataChannel* channel = new cricket::DataChannel(
      thread, ch, transport_controller, cricket::CN_DATA, rtcp);
  if (!channel->Init()) {
    delete channel;
    channel = NULL;
  }
  return channel;
}

template<>
void ChannelTest<DataTraits>::CreateContent(
    int flags,
    const cricket::AudioCodec& audio_codec,
    const cricket::VideoCodec& video_codec,
    cricket::DataContentDescription* data) {
  data->AddCodec(kGoogleDataCodec);
  data->set_rtcp_mux((flags & RTCP_MUX) != 0);
  if (flags & SECURE) {
    data->AddCrypto(cricket::CryptoParams(
        1, rtc::CS_AES_CM_128_HMAC_SHA1_32,
        "inline:" + rtc::CreateRandomString(40), std::string()));
  }
}

template<>
void ChannelTest<DataTraits>::CopyContent(
    const cricket::DataContentDescription& source,
    cricket::DataContentDescription* data) {
  *data = source;
}

template<>
bool ChannelTest<DataTraits>::CodecMatches(const cricket::DataCodec& c1,
                                           const cricket::DataCodec& c2) {
  return c1.name == c2.name;
}

template <>
void ChannelTest<DataTraits>::AddLegacyStreamInContent(
    uint32_t ssrc,
    int flags,
    cricket::DataContentDescription* data) {
  data->AddLegacyStream(ssrc);
}

TEST_F(DataChannelTest, TestInit) {
  Base::TestInit();
  EXPECT_FALSE(media_channel1_->IsStreamMuted(0));
}

TEST_F(DataChannelTest, TestSetContents) {
  Base::TestSetContents();
}

TEST_F(DataChannelTest, TestSetContentsNullOffer) {
  Base::TestSetContentsNullOffer();
}

TEST_F(DataChannelTest, TestSetContentsRtcpMux) {
  Base::TestSetContentsRtcpMux();
}

TEST_F(DataChannelTest, TestSetRemoteContentUpdate) {
  Base::TestSetRemoteContentUpdate();
}

TEST_F(DataChannelTest, TestStreams) {
  Base::TestStreams();
}

TEST_F(DataChannelTest, TestUpdateStreamsInLocalContent) {
  Base::TestUpdateStreamsInLocalContent();
}

TEST_F(DataChannelTest, TestUpdateRemoteStreamsInContent) {
  Base::TestUpdateStreamsInRemoteContent();
}

TEST_F(DataChannelTest, TestChangeStreamParamsInContent) {
  Base::TestChangeStreamParamsInContent();
}

TEST_F(DataChannelTest, TestPlayoutAndSendingStates) {
  Base::TestPlayoutAndSendingStates();
}

TEST_F(DataChannelTest, TestMediaContentDirection) {
  Base::TestMediaContentDirection();
}

TEST_F(DataChannelTest, TestCallSetup) {
  Base::TestCallSetup();
}

TEST_F(DataChannelTest, TestCallTeardownRtcpMux) {
  Base::TestCallTeardownRtcpMux();
}

TEST_F(DataChannelTest, TestOnReadyToSend) {
  Base::TestOnReadyToSend();
}

TEST_F(DataChannelTest, TestOnReadyToSendWithRtcpMux) {
  Base::TestOnReadyToSendWithRtcpMux();
}

TEST_F(DataChannelTest, SendRtpToRtp) {
  Base::SendRtpToRtp();
}

TEST_F(DataChannelTest, SendNoRtcpToNoRtcp) {
  Base::SendNoRtcpToNoRtcp();
}

TEST_F(DataChannelTest, SendNoRtcpToRtcp) {
  Base::SendNoRtcpToRtcp();
}

TEST_F(DataChannelTest, SendRtcpToNoRtcp) {
  Base::SendRtcpToNoRtcp();
}

TEST_F(DataChannelTest, SendRtcpToRtcp) {
  Base::SendRtcpToRtcp();
}

TEST_F(DataChannelTest, SendRtcpMuxToRtcp) {
  Base::SendRtcpMuxToRtcp();
}

TEST_F(DataChannelTest, SendRtcpMuxToRtcpMux) {
  Base::SendRtcpMuxToRtcpMux();
}

TEST_F(DataChannelTest, SendEarlyRtcpMuxToRtcp) {
  Base::SendEarlyRtcpMuxToRtcp();
}

TEST_F(DataChannelTest, SendEarlyRtcpMuxToRtcpMux) {
  Base::SendEarlyRtcpMuxToRtcpMux();
}

TEST_F(DataChannelTest, SendSrtpToSrtp) {
  Base::SendSrtpToSrtp();
}

TEST_F(DataChannelTest, SendSrtpToRtp) {
  Base::SendSrtpToSrtp();
}

TEST_F(DataChannelTest, SendSrtcpMux) {
  Base::SendSrtpToSrtp(RTCP_MUX, RTCP_MUX);
}

TEST_F(DataChannelTest, SendRtpToRtpOnThread) {
  Base::SendRtpToRtpOnThread();
}

TEST_F(DataChannelTest, SendSrtpToSrtpOnThread) {
  Base::SendSrtpToSrtpOnThread();
}

TEST_F(DataChannelTest, SendWithWritabilityLoss) {
  Base::SendWithWritabilityLoss();
}

TEST_F(DataChannelTest, TestMediaMonitor) {
  Base::TestMediaMonitor();
}

TEST_F(DataChannelTest, TestSendData) {
  CreateChannels(0, 0);
  EXPECT_TRUE(SendInitiate());
  EXPECT_TRUE(SendAccept());

  cricket::SendDataParams params;
  params.ssrc = 42;
  unsigned char data[] = {
    'f', 'o', 'o'
  };
  rtc::Buffer payload(data, 3);
  cricket::SendDataResult result;
  ASSERT_TRUE(media_channel1_->SendData(params, payload, &result));
  EXPECT_EQ(params.ssrc,
            media_channel1_->last_sent_data_params().ssrc);
  EXPECT_EQ("foo", media_channel1_->last_sent_data());
}

// TODO(pthatcher): TestSetReceiver?
