/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>  // max

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/configurable_frame_size_encoder.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/null_transport.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

enum VideoFormat { kGeneric, kVP8, };

class VideoSendStreamTest : public ::testing::Test {
 public:
  VideoSendStreamTest()
      : send_stream_(NULL), fake_encoder_(Clock::GetRealTimeClock()) {}

 protected:
  void RunSendTest(Call* call,
                   const VideoSendStream::Config& config,
                   test::RtpRtcpObserver* observer) {
    send_stream_ = call->CreateVideoSendStream(config);
    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
        test::FrameGeneratorCapturer::Create(
            send_stream_->Input(), 320, 240, 30, Clock::GetRealTimeClock()));
    send_stream_->Start();
    frame_generator_capturer->Start();

    EXPECT_EQ(kEventSignaled, observer->Wait());

    observer->StopSending();
    frame_generator_capturer->Stop();
    send_stream_->Stop();
    call->DestroyVideoSendStream(send_stream_);
  }

  VideoSendStream::Config GetSendTestConfig(Call* call, size_t num_streams) {
    assert(num_streams <= kNumSendSsrcs);
    VideoSendStream::Config config = call->GetDefaultSendConfig();
    config.encoder_settings = test::CreateEncoderSettings(
        &fake_encoder_, "FAKE", kFakeSendPayloadType, num_streams);
    config.encoder_settings.encoder = &fake_encoder_;
    config.encoder_settings.payload_type = kFakeSendPayloadType;
    for (size_t i = 0; i < num_streams; ++i)
      config.rtp.ssrcs.push_back(kSendSsrcs[i]);
    config.pacing = true;
    return config;
  }

  void TestNackRetransmission(uint32_t retransmit_ssrc,
                              uint8_t retransmit_payload_type,
                              bool enable_pacing);

  void TestPacketFragmentationSize(VideoFormat format, bool with_fec);

  void SendsSetSsrcs(size_t num_ssrcs, bool send_single_ssrc_first);

  enum { kNumSendSsrcs = 3 };
  static const uint8_t kSendPayloadType;
  static const uint8_t kSendRtxPayloadType;
  static const uint8_t kFakeSendPayloadType;
  static const uint32_t kSendSsrc;
  static const uint32_t kSendRtxSsrc;
  static const uint32_t kSendSsrcs[kNumSendSsrcs];

  VideoSendStream* send_stream_;
  test::FakeEncoder fake_encoder_;
};

const uint8_t VideoSendStreamTest::kSendPayloadType = 100;
const uint8_t VideoSendStreamTest::kFakeSendPayloadType = 125;
const uint8_t VideoSendStreamTest::kSendRtxPayloadType = 98;
const uint32_t VideoSendStreamTest::kSendRtxSsrc = 0xBADCAFE;
const uint32_t VideoSendStreamTest::kSendSsrcs[kNumSendSsrcs] = {
    0xC0FFED, 0xC0FFEE, 0xC0FFEF};
const uint32_t VideoSendStreamTest::kSendSsrc =
    VideoSendStreamTest::kSendSsrcs[0];

void VideoSendStreamTest::SendsSetSsrcs(size_t num_ssrcs,
                                        bool send_single_ssrc_first) {
  class SendSsrcObserver : public test::RtpRtcpObserver {
   public:
    SendSsrcObserver(const uint32_t* ssrcs,
                     size_t num_ssrcs,
                     bool send_single_ssrc_first)
        : RtpRtcpObserver(30 * 1000),
          ssrcs_to_observe_(num_ssrcs),
          expect_single_ssrc_(send_single_ssrc_first) {
      for (size_t i = 0; i < num_ssrcs; ++i)
        valid_ssrcs_[ssrcs[i]] = true;
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      // TODO(pbos): Reenable this part of the test when #1695 is resolved and
      //             all SSRCs are allocated on startup. This test was observed
      //             to fail on TSan as the codec gets set before the SSRCs are
      //             set up and some frames are sent on a random-generated SSRC
      //             before the correct SSRC gets set.
      // EXPECT_TRUE(valid_ssrcs_[header.ssrc])
      //    << "Received unknown SSRC: " << header.ssrc;
      //
      // if (!valid_ssrcs_[header.ssrc])
      //  observation_complete_->Set();

      if (!is_observed_[header.ssrc]) {
        is_observed_[header.ssrc] = true;
        --ssrcs_to_observe_;
        if (expect_single_ssrc_) {
          expect_single_ssrc_ = false;
          observation_complete_->Set();
        }
      }

      if (ssrcs_to_observe_ == 0)
        observation_complete_->Set();

      return SEND_PACKET;
    }

   private:
    std::map<uint32_t, bool> valid_ssrcs_;
    std::map<uint32_t, bool> is_observed_;
    size_t ssrcs_to_observe_;
    bool expect_single_ssrc_;
  } observer(kSendSsrcs, num_ssrcs, send_single_ssrc_first);

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config =
      GetSendTestConfig(call.get(), num_ssrcs);

  if (num_ssrcs > 1) {
    // Set low simulcast bitrates to not have to wait for bandwidth ramp-up.
    std::vector<VideoStream>* streams = &send_config.encoder_settings.streams;
    for (size_t i = 0; i < streams->size(); ++i) {
      (*streams)[i].min_bitrate_bps = 10000;
      (*streams)[i].target_bitrate_bps = 10000;
      (*streams)[i].max_bitrate_bps = 10000;
    }
  }

  std::vector<VideoStream> all_streams = send_config.encoder_settings.streams;
  if (send_single_ssrc_first)
    send_config.encoder_settings.streams.resize(1);

  send_stream_ = call->CreateVideoSendStream(send_config);
  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream_->Input(), 320, 240, 30, Clock::GetRealTimeClock()));
  send_stream_->Start();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for "
      << (send_single_ssrc_first ? "first SSRC." : "SSRCs.");

  if (send_single_ssrc_first) {
    // Set full simulcast and continue with the rest of the SSRCs.
    send_stream_->ReconfigureVideoEncoder(all_streams, NULL);
    EXPECT_EQ(kEventSignaled, observer.Wait())
        << "Timed out while waiting on additional SSRCs.";
  }

  observer.StopSending();
  frame_generator_capturer->Stop();
  send_stream_->Stop();
  call->DestroyVideoSendStream(send_stream_);
}

TEST_F(VideoSendStreamTest, CanStartStartedStream) {
  test::NullTransport transport;
  Call::Config call_config(&transport);
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config config = GetSendTestConfig(call.get(), 1);
  VideoSendStream* stream = call->CreateVideoSendStream(config);
  stream->Start();
  stream->Start();
  call->DestroyVideoSendStream(stream);
}

TEST_F(VideoSendStreamTest, CanStopStoppedStream) {
  test::NullTransport transport;
  Call::Config call_config(&transport);
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config config = GetSendTestConfig(call.get(), 1);
  VideoSendStream* stream = call->CreateVideoSendStream(config);
  stream->Stop();
  stream->Stop();
  call->DestroyVideoSendStream(stream);
}

TEST_F(VideoSendStreamTest, SendsSetSsrc) { SendsSetSsrcs(1, false); }

TEST_F(VideoSendStreamTest, DISABLED_SendsSetSimulcastSsrcs) {
  SendsSetSsrcs(kNumSendSsrcs, false);
}

TEST_F(VideoSendStreamTest, DISABLED_CanSwitchToUseAllSsrcs) {
  SendsSetSsrcs(kNumSendSsrcs, true);
}

TEST_F(VideoSendStreamTest, SupportsCName) {
  static std::string kCName = "PjQatC14dGfbVwGPUOA9IH7RlsFDbWl4AhXEiDsBizo=";
  class CNameObserver : public test::RtpRtcpObserver {
   public:
    CNameObserver() : RtpRtcpObserver(30 * 1000) {}

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        if (packet_type == RTCPUtility::kRtcpSdesChunkCode) {
          EXPECT_EQ(parser.Packet().CName.CName, kCName);
          observation_complete_->Set();
        }

        packet_type = parser.Iterate();
      }

      return SEND_PACKET;
    }
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.c_name = kCName;

  RunSendTest(call.get(), send_config, &observer);
}

TEST_F(VideoSendStreamTest, SupportsAbsoluteSendTime) {
  static const uint8_t kAbsSendTimeExtensionId = 13;
  class AbsoluteSendTimeObserver : public test::RtpRtcpObserver {
   public:
    AbsoluteSendTimeObserver() : RtpRtcpObserver(30 * 1000) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, kAbsSendTimeExtensionId));
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
      EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
      EXPECT_EQ(header.extension.transmissionTimeOffset, 0);
      EXPECT_GT(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));

  RunSendTest(call.get(), send_config, &observer);
}

TEST_F(VideoSendStreamTest, SupportsTransmissionTimeOffset) {
  static const uint8_t kTOffsetExtensionId = 13;
  class DelayedEncoder : public test::FakeEncoder {
   public:
    explicit DelayedEncoder(Clock* clock) : test::FakeEncoder(clock) {}
    virtual int32_t Encode(const I420VideoFrame& input_image,
                           const CodecSpecificInfo* codec_specific_info,
                           const std::vector<VideoFrameType>* frame_types)
        OVERRIDE {
      // A delay needs to be introduced to assure that we get a timestamp
      // offset.
      SleepMs(5);
      return FakeEncoder::Encode(input_image, codec_specific_info, frame_types);
    }
  } encoder(Clock::GetRealTimeClock());

  class TransmissionTimeOffsetObserver : public test::RtpRtcpObserver {
   public:
    TransmissionTimeOffsetObserver() : RtpRtcpObserver(30 * 1000) {
      EXPECT_TRUE(parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, kTOffsetExtensionId));
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
      EXPECT_FALSE(header.extension.hasAbsoluteSendTime);
      EXPECT_GT(header.extension.transmissionTimeOffset, 0);
      EXPECT_EQ(header.extension.absoluteSendTime, 0u);
      observation_complete_->Set();

      return SEND_PACKET;
    }
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.encoder_settings.encoder = &encoder;
  send_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTOffset, kTOffsetExtensionId));

  RunSendTest(call.get(), send_config, &observer);
}

class FakeReceiveStatistics : public NullReceiveStatistics {
 public:
  FakeReceiveStatistics(uint32_t send_ssrc,
                        uint32_t last_sequence_number,
                        uint32_t cumulative_lost,
                        uint8_t fraction_lost)
      : lossy_stats_(new LossyStatistician(last_sequence_number,
                                           cumulative_lost,
                                           fraction_lost)) {
    stats_map_[send_ssrc] = lossy_stats_.get();
  }

  virtual StatisticianMap GetActiveStatisticians() const OVERRIDE {
    return stats_map_;
  }

  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const OVERRIDE {
    return lossy_stats_.get();
  }

 private:
  class LossyStatistician : public StreamStatistician {
   public:
    LossyStatistician(uint32_t extended_max_sequence_number,
                      uint32_t cumulative_lost,
                      uint8_t fraction_lost) {
      stats_.fraction_lost = fraction_lost;
      stats_.cumulative_lost = cumulative_lost;
      stats_.extended_max_sequence_number = extended_max_sequence_number;
    }
    virtual bool GetStatistics(RtcpStatistics* statistics,
                               bool reset) OVERRIDE {
      *statistics = stats_;
      return true;
    }
    virtual void GetDataCounters(uint32_t* bytes_received,
                                 uint32_t* packets_received) const OVERRIDE {
      *bytes_received = 0;
      *packets_received = 0;
    }
    virtual uint32_t BitrateReceived() const OVERRIDE { return 0; }
    virtual void ResetStatistics() OVERRIDE {}
    virtual bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                         int min_rtt) const OVERRIDE {
      return false;
    }

    virtual bool IsPacketInOrder(uint16_t sequence_number) const OVERRIDE {
      return true;
    }

    RtcpStatistics stats_;
  };

  scoped_ptr<LossyStatistician> lossy_stats_;
  StatisticianMap stats_map_;
};

TEST_F(VideoSendStreamTest, SwapsI420VideoFrames) {
  static const size_t kWidth = 320;
  static const size_t kHeight = 240;

  test::NullTransport transport;
  Call::Config call_config(&transport);
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  VideoSendStream* video_send_stream = call->CreateVideoSendStream(send_config);
  video_send_stream->Start();

  I420VideoFrame frame;
  frame.CreateEmptyFrame(
      kWidth, kHeight, kWidth, (kWidth + 1) / 2, (kWidth + 1) / 2);
  uint8_t* old_y_buffer = frame.buffer(kYPlane);

  video_send_stream->Input()->SwapFrame(&frame);

  EXPECT_NE(frame.buffer(kYPlane), old_y_buffer);

  call->DestroyVideoSendStream(video_send_stream);
}

TEST_F(VideoSendStreamTest, SupportsFec) {
  static const int kRedPayloadType = 118;
  static const int kUlpfecPayloadType = 119;
  class FecObserver : public test::RtpRtcpObserver {
   public:
    FecObserver()
        : RtpRtcpObserver(30 * 1000),
          transport_adapter_(SendTransport()),
          send_count_(0),
          received_media_(false),
          received_fec_(false) {
      transport_adapter_.Enable();
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      // Send lossy receive reports to trigger FEC enabling.
      if (send_count_++ % 2 != 0) {
        // Receive statistics reporting having lost 50% of the packets.
        FakeReceiveStatistics lossy_receive_stats(
            kSendSsrc, header.sequenceNumber, send_count_ / 2, 127);
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &lossy_receive_stats);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrc);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      }

      EXPECT_EQ(kRedPayloadType, header.payloadType);

      uint8_t encapsulated_payload_type = packet[header.headerLength];

      if (encapsulated_payload_type == kUlpfecPayloadType) {
        received_fec_ = true;
      } else {
        received_media_ = true;
      }

      if (received_media_ && received_fec_)
        observation_complete_->Set();

      return SEND_PACKET;
    }

   private:
    internal::TransportAdapter transport_adapter_;
    int send_count_;
    bool received_media_;
    bool received_fec_;
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  observer.SetReceivers(call->Receiver(), NULL);

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.fec.red_payload_type = kRedPayloadType;
  send_config.rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;

  RunSendTest(call.get(), send_config, &observer);
}

void VideoSendStreamTest::TestNackRetransmission(
    uint32_t retransmit_ssrc,
    uint8_t retransmit_payload_type,
    bool enable_pacing) {
  class NackObserver : public test::RtpRtcpObserver {
   public:
    explicit NackObserver(uint32_t retransmit_ssrc,
                          uint8_t retransmit_payload_type)
        : RtpRtcpObserver(30 * 1000),
          transport_adapter_(SendTransport()),
          send_count_(0),
          retransmit_ssrc_(retransmit_ssrc),
          retransmit_payload_type_(retransmit_payload_type),
          nacked_sequence_number_(-1) {
      transport_adapter_.Enable();
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

      // Nack second packet after receiving the third one.
      if (++send_count_ == 3) {
        uint16_t nack_sequence_number = header.sequenceNumber - 1;
        nacked_sequence_number_ = nack_sequence_number;
        NullReceiveStatistics null_stats;
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &null_stats);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrc);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0,
                  rtcp_sender.SendRTCP(
                      feedback_state, kRtcpNack, 1, &nack_sequence_number));
      }

      uint16_t sequence_number = header.sequenceNumber;

      if (header.ssrc == retransmit_ssrc_ && retransmit_ssrc_ != kSendSsrc) {
        // Not kSendSsrc, assume correct RTX packet. Extract sequence number.
        const uint8_t* rtx_header = packet + header.headerLength;
        sequence_number = (rtx_header[0] << 8) + rtx_header[1];
      }

      if (sequence_number == nacked_sequence_number_) {
        EXPECT_EQ(retransmit_ssrc_, header.ssrc);
        EXPECT_EQ(retransmit_payload_type_, header.payloadType);
        observation_complete_->Set();
      }

      return SEND_PACKET;
    }

   private:
    internal::TransportAdapter transport_adapter_;
    int send_count_;
    uint32_t retransmit_ssrc_;
    uint8_t retransmit_payload_type_;
    int nacked_sequence_number_;
  } observer(retransmit_ssrc, retransmit_payload_type);

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));
  observer.SetReceivers(call->Receiver(), NULL);

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.nack.rtp_history_ms = 1000;
  send_config.rtp.rtx.payload_type = retransmit_payload_type;
  send_config.pacing = enable_pacing;
  if (retransmit_ssrc != kSendSsrc)
    send_config.rtp.rtx.ssrcs.push_back(retransmit_ssrc);

  RunSendTest(call.get(), send_config, &observer);
}

TEST_F(VideoSendStreamTest, RetransmitsNack) {
  // Normal NACKs should use the send SSRC.
  TestNackRetransmission(kSendSsrc, kFakeSendPayloadType, false);
}

TEST_F(VideoSendStreamTest, RetransmitsNackOverRtx) {
  // NACKs over RTX should use a separate SSRC.
  TestNackRetransmission(kSendRtxSsrc, kSendRtxPayloadType, false);
}

TEST_F(VideoSendStreamTest, RetransmitsNackOverRtxWithPacing) {
  // NACKs over RTX should use a separate SSRC.
  TestNackRetransmission(kSendRtxSsrc, kSendRtxPayloadType, true);
}

void VideoSendStreamTest::TestPacketFragmentationSize(VideoFormat format,
                                                      bool with_fec) {
  static const int kRedPayloadType = 118;
  static const int kUlpfecPayloadType = 119;
  // Observer that verifies that the expected number of packets and bytes
  // arrive for each frame size, from start_size to stop_size.
  class FrameFragmentationObserver : public test::RtpRtcpObserver,
                                     public EncodedFrameObserver {
   public:
    FrameFragmentationObserver(uint32_t max_packet_size,
                               uint32_t start_size,
                               uint32_t stop_size,
                               test::ConfigurableFrameSizeEncoder* encoder,
                               bool test_generic_packetization,
                               bool use_fec)
        : RtpRtcpObserver(120 * 1000),  // Timeout after two minutes.
          transport_adapter_(SendTransport()),
          encoder_(encoder),
          max_packet_size_(max_packet_size),
          stop_size_(stop_size),
          test_generic_packetization_(test_generic_packetization),
          use_fec_(use_fec),
          packet_count_(0),
          accumulated_size_(0),
          accumulated_payload_(0),
          fec_packet_received_(false),
          current_size_rtp_(start_size),
          current_size_frame_(start_size) {
      // Fragmentation required, this test doesn't make sense without it.
      assert(stop_size > max_packet_size);
      transport_adapter_.Enable();
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t size) OVERRIDE {
      uint32_t length = static_cast<int>(size);
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));

      EXPECT_LE(length, max_packet_size_);

      if (use_fec_) {
        uint8_t payload_type = packet[header.headerLength];
        bool is_fec = header.payloadType == kRedPayloadType &&
                      payload_type == kUlpfecPayloadType;
        if (is_fec) {
          fec_packet_received_ = true;
          return SEND_PACKET;
        }
      }

      accumulated_size_ += length;

      if (use_fec_)
        TriggerLossReport(header);

      if (test_generic_packetization_) {
        uint32_t overhead = header.headerLength + header.paddingLength +
                          (1 /* Generic header */);
        if (use_fec_)
          overhead += 1;  // RED for FEC header.
        accumulated_payload_ += length - overhead;
      }

      // Marker bit set indicates last packet of a frame.
      if (header.markerBit) {
        if (use_fec_ && accumulated_payload_ == current_size_rtp_ - 1) {
          // With FEC enabled, frame size is incremented asynchronously, so
          // "old" frames one byte too small may arrive. Accept, but don't
          // increase expected frame size.
          accumulated_size_ = 0;
          accumulated_payload_ = 0;
          return SEND_PACKET;
        }

        EXPECT_GE(accumulated_size_, current_size_rtp_);
        if (test_generic_packetization_) {
          EXPECT_EQ(current_size_rtp_, accumulated_payload_);
        }

        // Last packet of frame; reset counters.
        accumulated_size_ = 0;
        accumulated_payload_ = 0;
        if (current_size_rtp_ == stop_size_) {
          // Done! (Don't increase size again, might arrive more @ stop_size).
          observation_complete_->Set();
        } else {
          // Increase next expected frame size. If testing with FEC, make sure
          // a FEC packet has been received for this frame size before
          // proceeding, to make sure that redundancy packets don't exceed
          // size limit.
          if (!use_fec_) {
            ++current_size_rtp_;
          } else if (fec_packet_received_) {
            fec_packet_received_ = false;
            ++current_size_rtp_;
            ++current_size_frame_;
          }
        }
      }

      return SEND_PACKET;
    }

    void TriggerLossReport(const RTPHeader& header) {
      // Send lossy receive reports to trigger FEC enabling.
      if (packet_count_++ % 2 != 0) {
        // Receive statistics reporting having lost 50% of the packets.
        FakeReceiveStatistics lossy_receive_stats(
            kSendSsrc, header.sequenceNumber, packet_count_ / 2, 127);
        RTCPSender rtcp_sender(
            0, false, Clock::GetRealTimeClock(), &lossy_receive_stats);
        EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

        rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
        rtcp_sender.SetRemoteSSRC(kSendSsrc);

        RTCPSender::FeedbackState feedback_state;

        EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      }
    }

    virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) {
      // Increase frame size for next encoded frame, in the context of the
      // encoder thread.
      if (!use_fec_ &&
          current_size_frame_.Value() < static_cast<int32_t>(stop_size_)) {
        ++current_size_frame_;
      }
      encoder_->SetFrameSize(current_size_frame_.Value());
    }

   private:
    internal::TransportAdapter transport_adapter_;
    test::ConfigurableFrameSizeEncoder* const encoder_;

    const uint32_t max_packet_size_;
    const uint32_t stop_size_;
    const bool test_generic_packetization_;
    const bool use_fec_;

    uint32_t packet_count_;
    uint32_t accumulated_size_;
    uint32_t accumulated_payload_;
    bool fec_packet_received_;

    uint32_t current_size_rtp_;
    Atomic32 current_size_frame_;
  };

  // Use a fake encoder to output a frame of every size in the range [90, 290],
  // for each size making sure that the exact number of payload bytes received
  // is correct and that packets are fragmented to respect max packet size.
  static const uint32_t kMaxPacketSize = 128;
  static const uint32_t start = 90;
  static const uint32_t stop = 290;

  // Don't auto increment if FEC is used; continue sending frame size until
  // a FEC packet has been received.
  test::ConfigurableFrameSizeEncoder encoder(stop);
  encoder.SetFrameSize(start);

  FrameFragmentationObserver observer(
      kMaxPacketSize, start, stop, &encoder, format == kGeneric, with_fec);
  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  observer.SetReceivers(call->Receiver(), NULL);

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  if (with_fec) {
    send_config.rtp.fec.red_payload_type = kRedPayloadType;
    send_config.rtp.fec.ulpfec_payload_type = kUlpfecPayloadType;
  }

  if (format == kVP8)
    send_config.encoder_settings.payload_name = "VP8";

  send_config.pacing = false;
  send_config.encoder_settings.encoder = &encoder;
  send_config.rtp.max_packet_size = kMaxPacketSize;
  send_config.post_encode_callback = &observer;

  // Add an extension header, to make the RTP header larger than the base
  // length of 12 bytes.
  static const uint8_t kAbsSendTimeExtensionId = 13;
  send_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));

  RunSendTest(call.get(), send_config, &observer);
}

// TODO(sprang): Is there any way of speeding up these tests?
TEST_F(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kGeneric, false);
}

TEST_F(VideoSendStreamTest, FragmentsGenericAccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kGeneric, true);
}

TEST_F(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSize) {
  TestPacketFragmentationSize(kVP8, false);
}

TEST_F(VideoSendStreamTest, FragmentsVp8AccordingToMaxPacketSizeWithFec) {
  TestPacketFragmentationSize(kVP8, true);
}

// The test will go through a number of phases.
// 1. Start sending packets.
// 2. As soon as the RTP stream has been detected, signal a low REMB value to
//    suspend the stream.
// 3. Wait until |kSuspendTimeFrames| have been captured without seeing any RTP
//    packets.
// 4. Signal a high REMB and then wait for the RTP stream to start again.
//    When the stream is detected again, and the stats show that the stream
//    is no longer suspended, the test ends.
TEST_F(VideoSendStreamTest, SuspendBelowMinBitrate) {
  static const int kSuspendTimeFrames = 60;  // Suspend for 2 seconds @ 30 fps.

  class RembObserver : public test::RtpRtcpObserver, public I420FrameCallback {
   public:
    RembObserver(VideoSendStream** send_stream_ptr)
        : RtpRtcpObserver(30 * 1000),  // Timeout after 30 seconds.
          transport_adapter_(&transport_),
          clock_(Clock::GetRealTimeClock()),
          send_stream_ptr_(send_stream_ptr),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          test_state_(kBeforeSuspend),
          rtp_count_(0),
          last_sequence_number_(0),
          suspended_frame_count_(0),
          low_remb_bps_(0),
          high_remb_bps_(0) {
      transport_adapter_.Enable();
    }

    void SetReceiver(PacketReceiver* receiver) {
      transport_.SetReceiver(receiver);
    }

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      // Receive statistics reporting having lost 0% of the packets.
      // This is needed for the send-side bitrate controller to work properly.
      CriticalSectionScoped lock(crit_.get());
      SendRtcpFeedback(0);  // REMB is only sent if value is > 0.
      return SEND_PACKET;
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      ++rtp_count_;
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));
      last_sequence_number_ = header.sequenceNumber;

      if (test_state_ == kBeforeSuspend) {
        // The stream has started. Try to suspend it.
        SendRtcpFeedback(low_remb_bps_);
        test_state_ = kDuringSuspend;
      } else if (test_state_ == kDuringSuspend) {
        if (header.paddingLength == 0) {
          // Received non-padding packet during suspension period. Reset the
          // counter.
          suspended_frame_count_ = 0;
        }
      } else if (test_state_ == kWaitingForPacket) {
        if (header.paddingLength == 0) {
          // Non-padding packet observed. Test is almost complete. Will just
          // have to wait for the stats to change.
          test_state_ = kWaitingForStats;
        }
      } else if (test_state_ == kWaitingForStats) {
        assert(*send_stream_ptr_);
        VideoSendStream::Stats stats = (*send_stream_ptr_)->GetStats();
        if (stats.suspended == false) {
          // Stats flipped to false. Test is complete.
          observation_complete_->Set();
        }
      }

      return SEND_PACKET;
    }

    // This method implements the I420FrameCallback.
    void FrameCallback(I420VideoFrame* video_frame) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      if (test_state_ == kDuringSuspend &&
          ++suspended_frame_count_ > kSuspendTimeFrames) {
        assert(*send_stream_ptr_);
        VideoSendStream::Stats stats = (*send_stream_ptr_)->GetStats();
        EXPECT_TRUE(stats.suspended);
        SendRtcpFeedback(high_remb_bps_);
        test_state_ = kWaitingForPacket;
      }
    }

    void set_low_remb_bps(int value) {
      CriticalSectionScoped lock(crit_.get());
      low_remb_bps_ = value;
    }

    void set_high_remb_bps(int value) {
      CriticalSectionScoped lock(crit_.get());
      high_remb_bps_ = value;
    }

    void Stop() { transport_.StopSending(); }

   private:
    enum TestState {
      kBeforeSuspend,
      kDuringSuspend,
      kWaitingForPacket,
      kWaitingForStats
    };

    virtual void SendRtcpFeedback(int remb_value)
        EXCLUSIVE_LOCKS_REQUIRED(crit_) {
      FakeReceiveStatistics receive_stats(
          kSendSsrc, last_sequence_number_, rtp_count_, 0);
      RTCPSender rtcp_sender(0, false, clock_, &receive_stats);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrc);
      if (remb_value > 0) {
        rtcp_sender.SetREMBStatus(true);
        rtcp_sender.SetREMBData(remb_value, 0, NULL);
      }
      RTCPSender::FeedbackState feedback_state;
      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
    }

    internal::TransportAdapter transport_adapter_;
    test::DirectTransport transport_;
    Clock* const clock_;
    VideoSendStream** const send_stream_ptr_;

    const scoped_ptr<CriticalSectionWrapper> crit_;
    TestState test_state_ GUARDED_BY(crit_);
    int rtp_count_ GUARDED_BY(crit_);
    int last_sequence_number_ GUARDED_BY(crit_);
    int suspended_frame_count_ GUARDED_BY(crit_);
    int low_remb_bps_ GUARDED_BY(crit_);
    int high_remb_bps_ GUARDED_BY(crit_);
  } observer(&send_stream_);
  // Note that |send_stream_| is created in RunSendTest(), called below. This
  // is why a pointer to |send_stream_| must be provided here.

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));
  observer.SetReceiver(call->Receiver());

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.nack.rtp_history_ms = 1000;
  send_config.pre_encode_callback = &observer;
  send_config.suspend_below_min_bitrate = true;
  int min_bitrate_bps = send_config.encoder_settings.streams[0].min_bitrate_bps;
  observer.set_low_remb_bps(min_bitrate_bps - 10000);
  int threshold_window = std::max(min_bitrate_bps / 10, 10000);
  ASSERT_GT(send_config.encoder_settings.streams[0].max_bitrate_bps,
            min_bitrate_bps + threshold_window + 5000);
  observer.set_high_remb_bps(min_bitrate_bps + threshold_window + 5000);

  RunSendTest(call.get(), send_config, &observer);
  observer.Stop();
}

TEST_F(VideoSendStreamTest, NoPaddingWhenVideoIsMuted) {
  class PacketObserver : public test::RtpRtcpObserver {
   public:
    PacketObserver()
        : RtpRtcpObserver(30 * 1000),  // Timeout after 30 seconds.
          clock_(Clock::GetRealTimeClock()),
          transport_adapter_(ReceiveTransport()),
          crit_(CriticalSectionWrapper::CreateCriticalSection()),
          last_packet_time_ms_(-1),
          capturer_(NULL) {
      transport_adapter_.Enable();
    }

    void SetCapturer(test::FrameGeneratorCapturer* capturer) {
      CriticalSectionScoped lock(crit_.get());
      capturer_ = capturer;
    }

    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      last_packet_time_ms_ = clock_->TimeInMilliseconds();
      capturer_->Stop();
      return SEND_PACKET;
    }

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      CriticalSectionScoped lock(crit_.get());
      const int kVideoMutedThresholdMs = 10000;
      if (last_packet_time_ms_ > 0 &&
          clock_->TimeInMilliseconds() - last_packet_time_ms_ >
              kVideoMutedThresholdMs)
        observation_complete_->Set();
      // Receive statistics reporting having lost 50% of the packets.
      FakeReceiveStatistics receive_stats(kSendSsrcs[0], 1, 1, 0);
      RTCPSender rtcp_sender(
          0, false, Clock::GetRealTimeClock(), &receive_stats);
      EXPECT_EQ(0, rtcp_sender.RegisterSendTransport(&transport_adapter_));

      rtcp_sender.SetRTCPStatus(kRtcpNonCompound);
      rtcp_sender.SetRemoteSSRC(kSendSsrcs[0]);

      RTCPSender::FeedbackState feedback_state;

      EXPECT_EQ(0, rtcp_sender.SendRTCP(feedback_state, kRtcpRr));
      return SEND_PACKET;
    }

   private:
    Clock* const clock_;
    internal::TransportAdapter transport_adapter_;
    const scoped_ptr<CriticalSectionWrapper> crit_;
    int64_t last_packet_time_ms_ GUARDED_BY(crit_);
    test::FrameGeneratorCapturer* capturer_ GUARDED_BY(crit_);
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));
  observer.SetReceivers(call->Receiver(), call->Receiver());

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 3);

  send_stream_ = call->CreateVideoSendStream(send_config);
  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream_->Input(), 320, 240, 30, Clock::GetRealTimeClock()));
  observer.SetCapturer(frame_generator_capturer.get());
  send_stream_->Start();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for RTP packets to stop being sent.";

  observer.StopSending();
  frame_generator_capturer->Stop();
  send_stream_->Stop();
  call->DestroyVideoSendStream(send_stream_);
}

TEST_F(VideoSendStreamTest, ProducesStats) {
  static const std::string kCName =
      "PjQatC14dGfbVwGPUOA9IH7RlsFDbWl4AhXEiDsBizo=";
  static const uint32_t kTimeoutMs = 30 * 1000;
  class StatsObserver : public test::RtpRtcpObserver {
   public:
    StatsObserver()
        : RtpRtcpObserver(kTimeoutMs),
          stream_(NULL),
          event_(EventWrapper::Create()) {}

    virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      event_->Set();

      return SEND_PACKET;
    }

    bool WaitForFilledStats() {
      Clock* clock = Clock::GetRealTimeClock();
      int64_t now = clock->TimeInMilliseconds();
      int64_t stop_time = now + kTimeoutMs;
      while (now < stop_time) {
        int64_t time_left = stop_time - now;
        if (time_left > 0 && event_->Wait(time_left) == kEventSignaled &&
            CheckStats()) {
          return true;
        }
        now = clock->TimeInMilliseconds();
      }
      return false;
    }

    bool CheckStats() {
      VideoSendStream::Stats stats = stream_->GetStats();
      // Check that all applicable data sources have been used.
      if (stats.input_frame_rate > 0 && stats.encode_frame_rate > 0 &&
          stats.avg_delay_ms > 0 && stats.c_name == kCName &&
          !stats.substreams.empty()) {
        uint32_t ssrc = stats.substreams.begin()->first;
        EXPECT_NE(
            config_.rtp.ssrcs.end(),
            std::find(
                config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end(), ssrc));
        // Check for data populated by various sources. RTCP excluded as this
        // data is received from remote side. Tested in call tests instead.
        const StreamStats& entry = stats.substreams[ssrc];
        if (entry.key_frames > 0u && entry.bitrate_bps > 0 &&
            entry.rtp_stats.packets > 0u) {
          return true;
        }
      }
      return false;
    }

    void SetConfig(const VideoSendStream::Config& config) { config_ = config; }

    void SetSendStream(VideoSendStream* stream) { stream_ = stream; }

    VideoSendStream* stream_;
    VideoSendStream::Config config_;
    scoped_ptr<EventWrapper> event_;
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.c_name = kCName;
  observer.SetConfig(send_config);

  send_stream_ = call->CreateVideoSendStream(send_config);
  observer.SetSendStream(send_stream_);
  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream_->Input(), 320, 240, 30, Clock::GetRealTimeClock()));
  send_stream_->Start();
  frame_generator_capturer->Start();

  EXPECT_TRUE(observer.WaitForFilledStats())
      << "Timed out waiting for filled statistics.";

  observer.StopSending();
  frame_generator_capturer->Stop();
  send_stream_->Stop();
  call->DestroyVideoSendStream(send_stream_);
}

// This test first observes "high" bitrate use at which point it sends a REMB to
// indicate that it should be lowered significantly. The test then observes that
// the bitrate observed is sinking well below the min-transmit-bitrate threshold
// to verify that the min-transmit bitrate respects incoming REMB.
//
// Note that the test starts at "high" bitrate and does not ramp up to "higher"
// bitrate since no receiver block or remb is sent in the initial phase.
TEST_F(VideoSendStreamTest, MinTransmitBitrateRespectsRemb) {
  static const int kMinTransmitBitrateBps = 400000;
  static const int kHighBitrateBps = 150000;
  static const int kRembBitrateBps = 80000;
  static const int kRembRespectedBitrateBps = 100000;
  class BitrateObserver: public test::RtpRtcpObserver, public PacketReceiver {
   public:
    BitrateObserver()
        : RtpRtcpObserver(30 * 1000),
          feedback_transport_(ReceiveTransport()),
          send_stream_(NULL),
          bitrate_capped_(false) {
      RtpRtcp::Configuration config;
      feedback_transport_.Enable();
      config.outgoing_transport = &feedback_transport_;
      rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
      rtp_rtcp_->SetREMBStatus(true);
      rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    }

    void SetSendStream(VideoSendStream* send_stream) {
      send_stream_ = send_stream;
    }

   private:
    virtual bool DeliverPacket(const uint8_t* packet, size_t length) {
      if (RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)))
        return true;

      RTPHeader header;
      if (!parser_->Parse(packet, static_cast<int>(length), &header))
        return true;
      assert(send_stream_ != NULL);
      VideoSendStream::Stats stats = send_stream_->GetStats();
      if (!stats.substreams.empty()) {
        EXPECT_EQ(1u, stats.substreams.size());
        int bitrate_bps = stats.substreams.begin()->second.bitrate_bps;
        test::PrintResult(
            "bitrate_stats_",
            "min_transmit_bitrate_low_remb",
            "bitrate_bps",
            static_cast<size_t>(bitrate_bps),
            "bps",
            false);
        if (bitrate_bps > kHighBitrateBps) {
          rtp_rtcp_->SetREMBData(kRembBitrateBps, 1, &header.ssrc);
          rtp_rtcp_->Process();
          bitrate_capped_ = true;
        } else if (bitrate_capped_ &&
                   bitrate_bps < kRembRespectedBitrateBps) {
          observation_complete_->Set();
        }
      }
      return true;
    }

    scoped_ptr<RtpRtcp> rtp_rtcp_;
    internal::TransportAdapter feedback_transport_;
    VideoSendStream* send_stream_;
    bool bitrate_capped_;
  } observer;

  Call::Config call_config(observer.SendTransport());
  scoped_ptr<Call> call(Call::Create(call_config));
  observer.SetReceivers(&observer, call->Receiver());

  VideoSendStream::Config send_config = GetSendTestConfig(call.get(), 1);
  send_config.rtp.min_transmit_bitrate_bps = kMinTransmitBitrateBps;
  send_stream_ = call->CreateVideoSendStream(send_config);
  observer.SetSendStream(send_stream_);

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream_->Input(), 320, 240, 30, Clock::GetRealTimeClock()));
  send_stream_->Start();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timeout while waiting for low bitrate stats after REMB.";

  observer.StopSending();
  frame_generator_capturer->Stop();
  send_stream_->Stop();
  call->DestroyVideoSendStream(send_stream_);
}

}  // namespace webrtc
