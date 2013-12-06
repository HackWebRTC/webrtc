/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <assert.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common_video/test/frame_generator.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/remote_bitrate_estimator/include/rtp_to_ntp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/perf_test.h"

namespace webrtc {

static unsigned int kDefaultTimeoutMs = 30 * 1000;
static unsigned int kLongTimeoutMs = 120 * 1000;
static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kReceiverLocalSsrc = 0x123456;
static const uint8_t kSendPayloadType = 125;

class CallTest : public ::testing::Test {
 public:
  CallTest()
      : send_stream_(NULL),
        receive_stream_(NULL),
        fake_encoder_(Clock::GetRealTimeClock()) {}

  ~CallTest() {
    EXPECT_EQ(NULL, send_stream_);
    EXPECT_EQ(NULL, receive_stream_);
  }

 protected:
  void CreateCalls(const Call::Config& sender_config,
                   const Call::Config& receiver_config) {
    sender_call_.reset(Call::Create(sender_config));
    receiver_call_.reset(Call::Create(receiver_config));
  }

  void CreateTestConfigs() {
    send_config_ = sender_call_->GetDefaultSendConfig();
    receive_config_ = receiver_call_->GetDefaultReceiveConfig();

    send_config_.rtp.ssrcs.push_back(kSendSsrc);
    send_config_.encoder = &fake_encoder_;
    send_config_.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config_.codec, 1);
    send_config_.codec.plType = kSendPayloadType;

    receive_config_.codecs.clear();
    receive_config_.codecs.push_back(send_config_.codec);
    ExternalVideoDecoder decoder;
    decoder.decoder = &fake_decoder_;
    decoder.payload_type = send_config_.codec.plType;
    receive_config_.external_decoders.push_back(decoder);
    receive_config_.rtp.remote_ssrc = send_config_.rtp.ssrcs[0];
    receive_config_.rtp.local_ssrc = kReceiverLocalSsrc;
  }

  void CreateStreams() {
    assert(send_stream_ == NULL);
    assert(receive_stream_ == NULL);

    send_stream_ = sender_call_->CreateVideoSendStream(send_config_);
    receive_stream_ = receiver_call_->CreateVideoReceiveStream(receive_config_);
  }

  void CreateFrameGenerator() {
    frame_generator_capturer_.reset(
        test::FrameGeneratorCapturer::Create(send_stream_->Input(),
                                             send_config_.codec.width,
                                             send_config_.codec.height,
                                             30,
                                             Clock::GetRealTimeClock()));
  }

  void StartSending() {
    receive_stream_->StartReceiving();
    send_stream_->StartSending();
    if (frame_generator_capturer_.get() != NULL)
      frame_generator_capturer_->Start();
  }

  void StopSending() {
    if (frame_generator_capturer_.get() != NULL)
      frame_generator_capturer_->Stop();
    if (send_stream_ != NULL)
      send_stream_->StopSending();
    if (receive_stream_ != NULL)
      receive_stream_->StopReceiving();
  }

  void DestroyStreams() {
    if (send_stream_ != NULL)
      sender_call_->DestroyVideoSendStream(send_stream_);
    if (receive_stream_ != NULL)
      receiver_call_->DestroyVideoReceiveStream(receive_stream_);
    send_stream_ = NULL;
    receive_stream_ = NULL;
  }

  void ReceivesPliAndRecovers(int rtp_history_ms);
  void RespectsRtcpMode(newapi::RtcpMode rtcp_mode);
  void PlaysOutAudioAndVideoInSync();

  scoped_ptr<Call> sender_call_;
  scoped_ptr<Call> receiver_call_;

  VideoSendStream::Config send_config_;
  VideoReceiveStream::Config receive_config_;

  VideoSendStream* send_stream_;
  VideoReceiveStream* receive_stream_;

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;

  test::FakeEncoder fake_encoder_;
  test::FakeDecoder fake_decoder_;
};

class NackObserver : public test::RtpRtcpObserver {
  static const int kNumberOfNacksToObserve = 4;
  static const int kInverseProbabilityToStartLossBurst = 20;
  static const int kMaxLossBurst = 10;

 public:
  NackObserver()
      : test::RtpRtcpObserver(kLongTimeoutMs),
        rtp_parser_(RtpHeaderParser::Create()),
        drop_burst_count_(0),
        sent_rtp_packets_(0),
        nacks_left_(kNumberOfNacksToObserve) {}

 private:
  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    EXPECT_FALSE(RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)));

    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));

    // Never drop retransmitted packets.
    if (dropped_packets_.find(header.sequenceNumber) !=
        dropped_packets_.end()) {
      retransmitted_packets_.insert(header.sequenceNumber);
      return SEND_PACKET;
    }

    // Enough NACKs received, stop dropping packets.
    if (nacks_left_ == 0) {
      ++sent_rtp_packets_;
      return SEND_PACKET;
    }

    // Still dropping packets.
    if (drop_burst_count_ > 0) {
      --drop_burst_count_;
      dropped_packets_.insert(header.sequenceNumber);
      return DROP_PACKET;
    }

    // Should we start dropping packets?
    if (sent_rtp_packets_ > 0 &&
        rand() % kInverseProbabilityToStartLossBurst == 0) {
      drop_burst_count_ = rand() % kMaxLossBurst;
      dropped_packets_.insert(header.sequenceNumber);
      return DROP_PACKET;
    }

    ++sent_rtp_packets_;
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    bool received_nack = false;
    RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
    while (packet_type != RTCPUtility::kRtcpNotValidCode) {
      if (packet_type == RTCPUtility::kRtcpRtpfbNackCode)
        received_nack = true;

      packet_type = parser.Iterate();
    }

    if (received_nack) {
      ReceivedNack();
    } else {
      RtcpWithoutNack();
    }
    return SEND_PACKET;
  }

 private:
  void ReceivedNack() {
    if (nacks_left_ > 0)
      --nacks_left_;
    rtcp_without_nack_count_ = 0;
  }

  void RtcpWithoutNack() {
    if (nacks_left_ > 0)
      return;
    ++rtcp_without_nack_count_;

    // All packets retransmitted and no recent NACKs.
    if (dropped_packets_.size() == retransmitted_packets_.size() &&
        rtcp_without_nack_count_ >= kRequiredRtcpsWithoutNack) {
      observation_complete_->Set();
    }
  }

  scoped_ptr<RtpHeaderParser> rtp_parser_;
  std::set<uint16_t> dropped_packets_;
  std::set<uint16_t> retransmitted_packets_;
  int drop_burst_count_;
  uint64_t sent_rtp_packets_;
  int nacks_left_;
  int rtcp_without_nack_count_;
  static const int kRequiredRtcpsWithoutNack = 2;
};

TEST_F(CallTest, UsesTraceCallback) {
  const unsigned int kSenderTraceFilter = kTraceDebug;
  const unsigned int kReceiverTraceFilter = kTraceDefault & (~kTraceDebug);
  class TraceObserver : public TraceCallback {
   public:
    explicit TraceObserver(unsigned int filter)
        : filter_(filter), messages_left_(50), done_(EventWrapper::Create()) {}

    virtual void Print(TraceLevel level,
                       const char* message,
                       int length) OVERRIDE {
      EXPECT_EQ(0u, level & (~filter_));
      if (--messages_left_ == 0)
        done_->Set();
    }

    EventTypeWrapper Wait() { return done_->Wait(kDefaultTimeoutMs); }

   private:
    unsigned int filter_;
    unsigned int messages_left_;
    scoped_ptr<EventWrapper> done_;
  } sender_trace(kSenderTraceFilter), receiver_trace(kReceiverTraceFilter);

  test::DirectTransport send_transport, receive_transport;
  Call::Config sender_call_config(&send_transport);
  sender_call_config.trace_callback = &sender_trace;
  sender_call_config.trace_filter = kSenderTraceFilter;
  Call::Config receiver_call_config(&receive_transport);
  receiver_call_config.trace_callback = &receiver_trace;
  receiver_call_config.trace_filter = kReceiverTraceFilter;
  CreateCalls(sender_call_config, receiver_call_config);
  send_transport.SetReceiver(receiver_call_->Receiver());
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  // Wait() waits for a couple of trace callbacks to occur.
  EXPECT_EQ(kEventSignaled, sender_trace.Wait());
  EXPECT_EQ(kEventSignaled, receiver_trace.Wait());

  StopSending();
  send_transport.StopSending();
  receive_transport.StopSending();
  DestroyStreams();

  // The TraceCallback instance MUST outlive Calls, destroy Calls explicitly.
  sender_call_.reset();
  receiver_call_.reset();
}

TEST_F(CallTest, TransmitsFirstFrame) {
  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int /*time_to_render_ms*/) OVERRIDE {
      event_->Set();
    }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

    scoped_ptr<EventWrapper> event_;
  } renderer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();
  receive_config_.renderer = &renderer;

  CreateStreams();
  StartSending();

  scoped_ptr<test::FrameGenerator> frame_generator(test::FrameGenerator::Create(
      send_config_.codec.width, send_config_.codec.height));
  send_stream_->Input()->PutFrame(frame_generator->NextFrame(), 0);

  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceiverUsesLocalSsrc) {
  class SyncRtcpObserver : public test::RtpRtcpObserver {
   public:
    SyncRtcpObserver() : test::RtpRtcpObserver(kDefaultTimeoutMs) {}

    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());
      uint32_t ssrc = 0;
      ssrc |= static_cast<uint32_t>(packet[4]) << 24;
      ssrc |= static_cast<uint32_t>(packet[5]) << 16;
      ssrc |= static_cast<uint32_t>(packet[6]) << 8;
      ssrc |= static_cast<uint32_t>(packet[7]) << 0;
      EXPECT_EQ(kReceiverLocalSsrc, ssrc);
      observation_complete_->Set();

      return SEND_PACKET;
    }
  } observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for a receiver RTCP packet to be sent.";

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceivesAndRetransmitsNack) {
  NackObserver observer;

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  int rtp_history_ms = 1000;
  send_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.rtp.nack.rtp_history_ms = rtp_history_ms;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  // Wait() waits for an event triggered when NACKs have been received, NACKed
  // packets retransmitted and frames rendered again.
  EXPECT_EQ(kEventSignaled, observer.Wait());

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, UsesFrameCallbacks) {
  static const int kWidth = 320;
  static const int kHeight = 240;

  class Renderer : public VideoRenderer {
   public:
    Renderer() : event_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int /*time_to_render_ms*/) OVERRIDE {
      EXPECT_EQ(0, *video_frame.buffer(kYPlane))
          << "Rendered frame should have zero luma which is applied by the "
             "pre-render callback.";
      event_->Set();
    }

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }
    scoped_ptr<EventWrapper> event_;
  } renderer;

  class TestFrameCallback : public I420FrameCallback {
   public:
    TestFrameCallback(int expected_luma_byte, int next_luma_byte)
        : event_(EventWrapper::Create()),
          expected_luma_byte_(expected_luma_byte),
          next_luma_byte_(next_luma_byte) {}

    EventTypeWrapper Wait() { return event_->Wait(kDefaultTimeoutMs); }

   private:
    virtual void FrameCallback(I420VideoFrame* frame) {
      EXPECT_EQ(kWidth, frame->width())
          << "Width not as expected, callback done before resize?";
      EXPECT_EQ(kHeight, frame->height())
          << "Height not as expected, callback done before resize?";

      // Previous luma specified, observed luma should be fairly close.
      if (expected_luma_byte_ != -1) {
        EXPECT_NEAR(expected_luma_byte_, *frame->buffer(kYPlane), 10);
      }

      memset(frame->buffer(kYPlane),
             next_luma_byte_,
             frame->allocated_size(kYPlane));

      event_->Set();
    }

    scoped_ptr<EventWrapper> event_;
    int expected_luma_byte_;
    int next_luma_byte_;
  };

  TestFrameCallback pre_encode_callback(-1, 255);  // Changes luma to 255.
  TestFrameCallback pre_render_callback(255, 0);  // Changes luma from 255 to 0.

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.encoder = NULL;
  send_config_.codec = sender_call_->GetVideoCodecs()[0];
  send_config_.codec.width = kWidth;
  send_config_.codec.height = kHeight;
  send_config_.pre_encode_callback = &pre_encode_callback;
  receive_config_.pre_render_callback = &pre_render_callback;
  receive_config_.renderer = &renderer;

  CreateStreams();
  StartSending();

  // Create frames that are smaller than the send width/height, this is done to
  // check that the callbacks are done after processing video.
  scoped_ptr<test::FrameGenerator> frame_generator(
      test::FrameGenerator::Create(kWidth / 2, kHeight / 2));
  send_stream_->Input()->PutFrame(frame_generator->NextFrame(), 0);

  EXPECT_EQ(kEventSignaled, pre_encode_callback.Wait())
      << "Timed out while waiting for pre-encode callback.";
  EXPECT_EQ(kEventSignaled, pre_render_callback.Wait())
      << "Timed out while waiting for pre-render callback.";
  EXPECT_EQ(kEventSignaled, renderer.Wait())
      << "Timed out while waiting for the frame to render.";

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}

class PliObserver : public test::RtpRtcpObserver, public VideoRenderer {
  static const int kInverseDropProbability = 16;

 public:
  explicit PliObserver(bool nack_enabled)
      : test::RtpRtcpObserver(kLongTimeoutMs),
        rtp_header_parser_(RtpHeaderParser::Create()),
        nack_enabled_(nack_enabled),
        first_retransmitted_timestamp_(0),
        last_send_timestamp_(0),
        rendered_frame_(false),
        received_pli_(false) {}

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    RTPHeader header;
    EXPECT_TRUE(
        rtp_header_parser_->Parse(packet, static_cast<int>(length), &header));

    // Drop all NACK retransmissions. This is to force transmission of a PLI.
    if (header.timestamp < last_send_timestamp_)
      return DROP_PACKET;

    if (received_pli_) {
      if (first_retransmitted_timestamp_ == 0) {
        first_retransmitted_timestamp_ = header.timestamp;
      }
    } else if (rendered_frame_ && rand() % kInverseDropProbability == 0) {
      return DROP_PACKET;
    }

    last_send_timestamp_ = header.timestamp;
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
         packet_type != RTCPUtility::kRtcpNotValidCode;
         packet_type = parser.Iterate()) {
      if (!nack_enabled_)
        EXPECT_NE(packet_type, RTCPUtility::kRtcpRtpfbNackCode);

      if (packet_type == RTCPUtility::kRtcpPsfbPliCode) {
        received_pli_ = true;
        break;
      }
    }
    return SEND_PACKET;
  }

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    CriticalSectionScoped crit_(lock_.get());
    if (first_retransmitted_timestamp_ != 0 &&
        video_frame.timestamp() > first_retransmitted_timestamp_) {
      EXPECT_TRUE(received_pli_);
      observation_complete_->Set();
    }
    rendered_frame_ = true;
  }

 private:
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  bool nack_enabled_;

  uint32_t first_retransmitted_timestamp_;
  uint32_t last_send_timestamp_;

  bool rendered_frame_;
  bool received_pli_;
};

void CallTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  PliObserver observer(rtp_history_ms > 0);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.renderer = &observer;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  // Wait() waits for an event triggered when Pli has been received and frames
  // have been rendered afterwards.
  EXPECT_EQ(kEventSignaled, observer.Wait());

  StopSending();

  observer.StopSending();

  DestroyStreams();
}

TEST_F(CallTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

// TODO(pbos): Enable this when 2250 is resolved.
TEST_F(CallTest, DISABLED_ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}

TEST_F(CallTest, SurvivesIncomingRtpPacketsToDestroyedReceiveStream) {
  class PacketInputObserver : public PacketReceiver {
   public:
    explicit PacketInputObserver(PacketReceiver* receiver)
        : receiver_(receiver), delivered_packet_(EventWrapper::Create()) {}

    EventTypeWrapper Wait() {
      return delivered_packet_->Wait(kDefaultTimeoutMs);
    }

   private:
    virtual bool DeliverPacket(const uint8_t* packet, size_t length) {
      if (RtpHeaderParser::IsRtcp(packet, static_cast<int>(length))) {
        return receiver_->DeliverPacket(packet, length);
      } else {
        EXPECT_FALSE(receiver_->DeliverPacket(packet, length));
        delivered_packet_->Set();
        return false;
      }
    }

    PacketReceiver* receiver_;
    scoped_ptr<EventWrapper> delivered_packet_;
  };

  test::DirectTransport send_transport, receive_transport;

  CreateCalls(Call::Config(&send_transport), Call::Config(&receive_transport));
  PacketInputObserver input_observer(receiver_call_->Receiver());

  send_transport.SetReceiver(&input_observer);
  receive_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  receiver_call_->DestroyVideoReceiveStream(receive_stream_);
  receive_stream_ = NULL;

  // Wait() waits for a received packet.
  EXPECT_EQ(kEventSignaled, input_observer.Wait());

  StopSending();

  DestroyStreams();

  send_transport.StopSending();
  receive_transport.StopSending();
}

void CallTest::RespectsRtcpMode(newapi::RtcpMode rtcp_mode) {
  static const int kRtpHistoryMs = 1000;
  static const int kNumCompoundRtcpPacketsToObserve = 10;
  class RtcpModeObserver : public test::RtpRtcpObserver {
   public:
    explicit RtcpModeObserver(newapi::RtcpMode rtcp_mode)
        : test::RtpRtcpObserver(kDefaultTimeoutMs),
          rtcp_mode_(rtcp_mode),
          sent_rtp_(0),
          sent_rtcp_(0) {}

   private:
    virtual Action OnSendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      if (++sent_rtp_ % 3 == 0)
        return DROP_PACKET;

      return SEND_PACKET;
    }

    virtual Action OnReceiveRtcp(const uint8_t* packet,
                                 size_t length) OVERRIDE {
      ++sent_rtcp_;
      RTCPUtility::RTCPParserV2 parser(packet, length, true);
      EXPECT_TRUE(parser.IsValid());

      RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
      bool has_report_block = false;
      while (packet_type != RTCPUtility::kRtcpNotValidCode) {
        EXPECT_NE(RTCPUtility::kRtcpSrCode, packet_type);
        if (packet_type == RTCPUtility::kRtcpRrCode) {
          has_report_block = true;
          break;
        }
        packet_type = parser.Iterate();
      }

      switch (rtcp_mode_) {
        case newapi::kRtcpCompound:
          if (!has_report_block) {
            ADD_FAILURE() << "Received RTCP packet without receiver report for "
                             "kRtcpCompound.";
            observation_complete_->Set();
          }

          if (sent_rtcp_ >= kNumCompoundRtcpPacketsToObserve)
            observation_complete_->Set();

          break;
        case newapi::kRtcpReducedSize:
          if (!has_report_block)
            observation_complete_->Set();
          break;
      }

      return SEND_PACKET;
    }

    newapi::RtcpMode rtcp_mode_;
    int sent_rtp_;
    int sent_rtcp_;
  } observer(rtcp_mode);

  CreateCalls(Call::Config(observer.SendTransport()),
              Call::Config(observer.ReceiveTransport()));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = kRtpHistoryMs;
  receive_config_.rtp.nack.rtp_history_ms = kRtpHistoryMs;
  receive_config_.rtp.rtcp_mode = rtcp_mode;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << (rtcp_mode == newapi::kRtcpCompound
              ? "Timed out before observing enough compound packets."
              : "Timed out before receiving a non-compound RTCP packet.");

  StopSending();
  observer.StopSending();
  DestroyStreams();
}

TEST_F(CallTest, UsesRtcpCompoundMode) {
  RespectsRtcpMode(newapi::kRtcpCompound);
}

TEST_F(CallTest, UsesRtcpReducedSizeMode) {
  RespectsRtcpMode(newapi::kRtcpReducedSize);
}

// Test sets up a Call multiple senders with different resolutions and SSRCs.
// Another is set up to receive all three of these with different renderers.
// Each renderer verifies that it receives the expected resolution, and as soon
// as every renderer has received a frame, the test finishes.
TEST_F(CallTest, SendsAndReceivesMultipleStreams) {
  static const size_t kNumStreams = 3;

  class VideoOutputObserver : public VideoRenderer {
   public:
    VideoOutputObserver(test::FrameGeneratorCapturer** capturer,
                        int width,
                        int height)
        : capturer_(capturer),
          width_(width),
          height_(height),
          done_(EventWrapper::Create()) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int time_to_render_ms) OVERRIDE {
      EXPECT_EQ(width_, video_frame.width());
      EXPECT_EQ(height_, video_frame.height());
      (*capturer_)->Stop();
      done_->Set();
    }

    void Wait() { done_->Wait(kDefaultTimeoutMs); }

   private:
    test::FrameGeneratorCapturer** capturer_;
    int width_;
    int height_;
    scoped_ptr<EventWrapper> done_;
  };

  struct {
    uint32_t ssrc;
    int width;
    int height;
  } codec_settings[kNumStreams] = {{1, 640, 480}, {2, 320, 240}, {3, 240, 160}};

  test::DirectTransport sender_transport, receiver_transport;
  scoped_ptr<Call> sender_call(Call::Create(Call::Config(&sender_transport)));
  scoped_ptr<Call> receiver_call(
      Call::Create(Call::Config(&receiver_transport)));
  sender_transport.SetReceiver(receiver_call->Receiver());
  receiver_transport.SetReceiver(sender_call->Receiver());

  VideoSendStream* send_streams[kNumStreams];
  VideoReceiveStream* receive_streams[kNumStreams];

  VideoOutputObserver* observers[kNumStreams];
  test::FrameGeneratorCapturer* frame_generators[kNumStreams];

  for (size_t i = 0; i < kNumStreams; ++i) {
    uint32_t ssrc = codec_settings[i].ssrc;
    int width = codec_settings[i].width;
    int height = codec_settings[i].height;
    observers[i] = new VideoOutputObserver(&frame_generators[i], width, height);

    VideoReceiveStream::Config receive_config =
        receiver_call->GetDefaultReceiveConfig();
    receive_config.renderer = observers[i];
    receive_config.rtp.remote_ssrc = ssrc;
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_streams[i] =
        receiver_call->CreateVideoReceiveStream(receive_config);
    receive_streams[i]->StartReceiving();

    VideoSendStream::Config send_config = sender_call->GetDefaultSendConfig();
    send_config.rtp.ssrcs.push_back(ssrc);
    send_config.codec.width = width;
    send_config.codec.height = height;
    send_streams[i] = sender_call->CreateVideoSendStream(send_config);
    send_streams[i]->StartSending();

    frame_generators[i] = test::FrameGeneratorCapturer::Create(
        send_streams[i]->Input(), width, height, 30, Clock::GetRealTimeClock());
    frame_generators[i]->Start();
  }

  for (size_t i = 0; i < kNumStreams; ++i) {
    observers[i]->Wait();
  }

  for (size_t i = 0; i < kNumStreams; ++i) {
    frame_generators[i]->Stop();
    delete frame_generators[i];
    sender_call->DestroyVideoSendStream(send_streams[i]);
    receiver_call->DestroyVideoReceiveStream(receive_streams[i]);
    delete observers[i];
  }

  sender_transport.StopSending();
  receiver_transport.StopSending();
}

class SyncRtcpObserver : public test::RtpRtcpObserver {
 public:
  explicit SyncRtcpObserver(int delay_ms)
      : test::RtpRtcpObserver(kLongTimeoutMs, delay_ms),
        critical_section_(CriticalSectionWrapper::CreateCriticalSection()) {}

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
         packet_type != RTCPUtility::kRtcpNotValidCode;
         packet_type = parser.Iterate()) {
      if (packet_type == RTCPUtility::kRtcpSrCode) {
        const RTCPUtility::RTCPPacket& packet = parser.Packet();
        synchronization::RtcpMeasurement ntp_rtp_pair(
            packet.SR.NTPMostSignificant,
            packet.SR.NTPLeastSignificant,
            packet.SR.RTPTimestamp);
        StoreNtpRtpPair(ntp_rtp_pair);
      }
    }
    return SEND_PACKET;
  }

  int64_t RtpTimestampToNtp(uint32_t timestamp) const {
    CriticalSectionScoped cs(critical_section_.get());
    int64_t timestamp_in_ms = -1;
    if (ntp_rtp_pairs_.size() == 2) {
      // TODO(stefan): We can't EXPECT_TRUE on this call due to a bug in the
      // RTCP sender where it sends RTCP SR before any RTP packets, which leads
      // to a bogus NTP/RTP mapping.
      synchronization::RtpToNtpMs(timestamp, ntp_rtp_pairs_, &timestamp_in_ms);
      return timestamp_in_ms;
    }
    return -1;
  }

 private:
  void StoreNtpRtpPair(synchronization::RtcpMeasurement ntp_rtp_pair) {
    CriticalSectionScoped cs(critical_section_.get());
    for (synchronization::RtcpList::iterator it = ntp_rtp_pairs_.begin();
         it != ntp_rtp_pairs_.end();
         ++it) {
      if (ntp_rtp_pair.ntp_secs == it->ntp_secs &&
          ntp_rtp_pair.ntp_frac == it->ntp_frac) {
        // This RTCP has already been added to the list.
        return;
      }
    }
    // We need two RTCP SR reports to map between RTP and NTP. More than two
    // will not improve the mapping.
    if (ntp_rtp_pairs_.size() == 2) {
      ntp_rtp_pairs_.pop_back();
    }
    ntp_rtp_pairs_.push_front(ntp_rtp_pair);
  }

  scoped_ptr<CriticalSectionWrapper> critical_section_;
  synchronization::RtcpList ntp_rtp_pairs_;
};

class VideoRtcpAndSyncObserver : public SyncRtcpObserver, public VideoRenderer {
  static const int kInSyncThresholdMs = 50;
  static const int kStartupTimeMs = 2000;
  static const int kMinRunTimeMs = 30000;

 public:
  VideoRtcpAndSyncObserver(Clock* clock,
                           int voe_channel,
                           VoEVideoSync* voe_sync,
                           SyncRtcpObserver* audio_observer)
      : SyncRtcpObserver(0),
        clock_(clock),
        voe_channel_(voe_channel),
        voe_sync_(voe_sync),
        audio_observer_(audio_observer),
        creation_time_ms_(clock_->TimeInMilliseconds()),
        first_time_in_sync_(-1) {}

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    int64_t now_ms = clock_->TimeInMilliseconds();
    uint32_t playout_timestamp = 0;
    if (voe_sync_->GetPlayoutTimestamp(voe_channel_, playout_timestamp) != 0)
      return;
    int64_t latest_audio_ntp =
        audio_observer_->RtpTimestampToNtp(playout_timestamp);
    int64_t latest_video_ntp = RtpTimestampToNtp(video_frame.timestamp());
    if (latest_audio_ntp < 0 || latest_video_ntp < 0)
      return;
    int time_until_render_ms =
        std::max(0, static_cast<int>(video_frame.render_time_ms() - now_ms));
    latest_video_ntp += time_until_render_ms;
    int64_t stream_offset = latest_audio_ntp - latest_video_ntp;
    std::stringstream ss;
    ss << stream_offset;
    webrtc::test::PrintResult(
        "stream_offset", "", "synchronization", ss.str(), "ms", false);
    int64_t time_since_creation = now_ms - creation_time_ms_;
    // During the first couple of seconds audio and video can falsely be
    // estimated as being synchronized. We don't want to trigger on those.
    if (time_since_creation < kStartupTimeMs)
      return;
    if (abs(latest_audio_ntp - latest_video_ntp) < kInSyncThresholdMs) {
      if (first_time_in_sync_ == -1) {
        first_time_in_sync_ = now_ms;
        webrtc::test::PrintResult("sync_convergence_time",
                                  "",
                                  "synchronization",
                                  time_since_creation,
                                  "ms",
                                  false);
      }
      if (time_since_creation > kMinRunTimeMs)
        observation_complete_->Set();
    }
  }

 private:
  Clock* clock_;
  int voe_channel_;
  VoEVideoSync* voe_sync_;
  SyncRtcpObserver* audio_observer_;
  int64_t creation_time_ms_;
  int64_t first_time_in_sync_;
};

TEST_F(CallTest, PlaysOutAudioAndVideoInSync) {
  VoiceEngine* voice_engine = VoiceEngine::Create();
  VoEBase* voe_base = VoEBase::GetInterface(voice_engine);
  VoECodec* voe_codec = VoECodec::GetInterface(voice_engine);
  VoENetwork* voe_network = VoENetwork::GetInterface(voice_engine);
  VoEVideoSync* voe_sync = VoEVideoSync::GetInterface(voice_engine);
  const std::string audio_filename =
      test::ResourcePath("voice_engine/audio_long16", "pcm");
  ASSERT_STRNE("", audio_filename.c_str());
  test::FakeAudioDevice fake_audio_device(Clock::GetRealTimeClock(),
                                          audio_filename);
  EXPECT_EQ(0, voe_base->Init(&fake_audio_device, NULL));
  int channel = voe_base->CreateChannel();

  const int kVoiceDelayMs = 500;
  SyncRtcpObserver audio_observer(kVoiceDelayMs);
  VideoRtcpAndSyncObserver observer(
      Clock::GetRealTimeClock(), channel, voe_sync, &audio_observer);

  Call::Config receiver_config(observer.ReceiveTransport());
  receiver_config.voice_engine = voice_engine;
  CreateCalls(Call::Config(observer.SendTransport()), receiver_config);
  CodecInst isac = {103, "ISAC", 16000, 480, 1, 32000};
  EXPECT_EQ(0, voe_codec->SetSendCodec(channel, isac));

  class VoicePacketReceiver : public PacketReceiver {
   public:
    VoicePacketReceiver(int channel, VoENetwork* voe_network)
        : channel_(channel),
          voe_network_(voe_network),
          parser_(RtpHeaderParser::Create()) {}
    virtual bool DeliverPacket(const uint8_t* packet, size_t length) {
      int ret;
      if (parser_->IsRtcp(packet, static_cast<int>(length))) {
        ret = voe_network_->ReceivedRTCPPacket(
            channel_, packet, static_cast<unsigned int>(length));
      } else {
        ret = voe_network_->ReceivedRTPPacket(
            channel_, packet, static_cast<unsigned int>(length));
      }
      return ret == 0;
    }

   private:
    int channel_;
    VoENetwork* voe_network_;
    scoped_ptr<RtpHeaderParser> parser_;
  } voe_packet_receiver(channel, voe_network);

  audio_observer.SetReceivers(&voe_packet_receiver, &voe_packet_receiver);

  internal::TransportAdapter transport_adapter(audio_observer.SendTransport());
  EXPECT_EQ(0,
            voe_network->RegisterExternalTransport(channel, transport_adapter));

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = 1000;
  receive_config_.rtp.nack.rtp_history_ms = 1000;
  receive_config_.renderer = &observer;
  receive_config_.audio_channel_id = channel;

  CreateStreams();
  CreateFrameGenerator();
  StartSending();

  fake_audio_device.Start();
  EXPECT_EQ(0, voe_base->StartPlayout(channel));
  EXPECT_EQ(0, voe_base->StartReceive(channel));
  EXPECT_EQ(0, voe_base->StartSend(channel));

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for audio and video to be synchronized.";

  EXPECT_EQ(0, voe_base->StopSend(channel));
  EXPECT_EQ(0, voe_base->StopReceive(channel));
  EXPECT_EQ(0, voe_base->StopPlayout(channel));
  fake_audio_device.Stop();

  StopSending();
  observer.StopSending();
  audio_observer.StopSending();

  voe_base->DeleteChannel(channel);
  voe_base->Release();
  voe_codec->Release();
  voe_network->Release();
  voe_sync->Release();
  DestroyStreams();
  VoiceEngine::Delete(voice_engine);
}

TEST_F(CallTest, ObserversEncodedFrames) {
  class EncodedFrameTestObserver : public EncodedFrameObserver {
   public:
    EncodedFrameTestObserver() : length_(0),
                                 frame_type_(kFrameEmpty),
                                 called_(EventWrapper::Create()) {}
    virtual ~EncodedFrameTestObserver() {}

    virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) {
      frame_type_ = encoded_frame.frame_type_;
      length_ = encoded_frame.length_;
      buffer_.reset(new uint8_t[length_]);
      memcpy(buffer_.get(), encoded_frame.data_, length_);
      called_->Set();
    }

    EventTypeWrapper Wait() {
      return called_->Wait(kDefaultTimeoutMs);
    }

    void ExpectEqualFrames(const EncodedFrameTestObserver& observer) {
      ASSERT_EQ(length_, observer.length_)
          << "Observed frames are of different lengths.";
      EXPECT_EQ(frame_type_, observer.frame_type_)
          << "Observed frames have different frame types.";
      EXPECT_EQ(0, memcmp(buffer_.get(), observer.buffer_.get(), length_))
          << "Observed encoded frames have different content.";
    }

   private:
    scoped_ptr<uint8_t[]> buffer_;
    size_t length_;
    FrameType frame_type_;
    scoped_ptr<EventWrapper> called_;
  };

  EncodedFrameTestObserver post_encode_observer;
  EncodedFrameTestObserver pre_decode_observer;

  test::DirectTransport sender_transport, receiver_transport;

  CreateCalls(Call::Config(&sender_transport),
              Call::Config(&receiver_transport));

  sender_transport.SetReceiver(receiver_call_->Receiver());
  receiver_transport.SetReceiver(sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.post_encode_callback = &post_encode_observer;
  receive_config_.pre_decode_callback = &pre_decode_observer;

  CreateStreams();
  StartSending();

  scoped_ptr<test::FrameGenerator> frame_generator(test::FrameGenerator::Create(
      send_config_.codec.width, send_config_.codec.height));
  send_stream_->Input()->PutFrame(frame_generator->NextFrame(), 0);

  EXPECT_EQ(kEventSignaled, post_encode_observer.Wait())
      << "Timed out while waiting for send-side encoded-frame callback.";

  EXPECT_EQ(kEventSignaled, pre_decode_observer.Wait())
      << "Timed out while waiting for pre-decode encoded-frame callback.";

  post_encode_observer.ExpectEqualFrames(pre_decode_observer);

  StopSending();

  sender_transport.StopSending();
  receiver_transport.StopSending();

  DestroyStreams();
}
}  // namespace webrtc
