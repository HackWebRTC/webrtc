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

#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/video_engine/new_include/video_call.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/fake_encoder.h"
#include "webrtc/video_engine/test/common/frame_generator.h"
#include "webrtc/video_engine/test/common/frame_generator_capturer.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"
#include "webrtc/video_engine/test/common/rtp_rtcp_observer.h"

namespace webrtc {

class StreamObserver : public newapi::Transport, public RemoteBitrateObserver {
 public:
  typedef std::map<uint32_t, int> BytesSentMap;
  StreamObserver(int num_expected_ssrcs, newapi::Transport* feedback_transport,
                 Clock* clock)
      : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
        all_ssrcs_sent_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(new TransportWrapper(feedback_transport)),
        receive_stats_(ReceiveStatistics::Create(clock)),
        clock_(clock),
        num_expected_ssrcs_(num_expected_ssrcs) {
    // Ideally we would only have to instantiate an RtcpSender, an
    // RtpHeaderParser and a RemoteBitrateEstimator here, but due to the current
    // state of the RTP module we need a full module and receive statistics to
    // be able to produce an RTCP with REMB.
    RtpRtcp::Configuration config;
    config.receive_statistics = receive_stats_.get();
    config.outgoing_transport = feedback_transport_.get();
    rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
    rtp_rtcp_->SetREMBStatus(true);
    rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                            1);
    AbsoluteSendTimeRemoteBitrateEstimatorFactory rbe_factory;
    remote_bitrate_estimator_.reset(rbe_factory.Create(this, clock));
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    CriticalSectionScoped lock(critical_section_.get());
    if (ssrcs.size() == num_expected_ssrcs_ && bitrate >= kExpectedBitrateBps)
      all_ssrcs_sent_->Set();
    rtp_rtcp_->SetREMBData(bitrate, static_cast<uint8_t>(ssrcs.size()),
                           &ssrcs[0]);
    rtp_rtcp_->Process();
  }

  virtual bool SendRTP(const uint8_t* packet, size_t length) OVERRIDE {
    CriticalSectionScoped lock(critical_section_.get());
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length),
                                   &header));
    receive_stats_->IncomingPacket(header, length, false, true);
    rtp_rtcp_->SetRemoteSSRC(header.ssrc);
    remote_bitrate_estimator_->IncomingPacket(clock_->TimeInMilliseconds(),
                                              static_cast<int>(length - 12),
                                              header);
    if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
      remote_bitrate_estimator_->Process();
    }
    return true;
  }

  virtual bool SendRTCP(const uint8_t* packet, size_t length) OVERRIDE {
    return true;
  }

  EventTypeWrapper Wait() {
    return all_ssrcs_sent_->Wait(120 * 1000);
  }

 private:
  class TransportWrapper : public webrtc::Transport {
   public:
    explicit TransportWrapper(newapi::Transport* new_transport)
        : new_transport_(new_transport) {}

    virtual int SendPacket(int channel, const void *data, int len) OVERRIDE {
      return new_transport_->SendRTP(static_cast<const uint8_t*>(data), len) ?
          len : -1;
    }

    virtual int SendRTCPPacket(int channel, const void *data,
                               int len) OVERRIDE {
      return new_transport_->SendRTCP(static_cast<const uint8_t*>(data), len) ?
          len : -1;
    }

   private:
    newapi::Transport* new_transport_;
  };

  static const unsigned int kExpectedBitrateBps = 1200000;

  scoped_ptr<CriticalSectionWrapper> critical_section_;
  scoped_ptr<EventWrapper> all_ssrcs_sent_;
  scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  scoped_ptr<TransportWrapper> feedback_transport_;
  scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  Clock* clock_;
  const size_t num_expected_ssrcs_;
};

class RampUpTest : public ::testing::TestWithParam<bool> {
 public:
  virtual void SetUp() {
    reserved_ssrcs_.clear();
  }

  static void SetCodecStreamSettings(VideoCodec* video_codec) {
    video_codec->width = 1280;
    video_codec->height = 720;
    video_codec->startBitrate = 300;
    video_codec->minBitrate = 50;
    video_codec->maxBitrate = 1800;

    video_codec->numberOfSimulcastStreams = 3;
    video_codec->simulcastStream[0].width = 320;
    video_codec->simulcastStream[0].height = 180;
    video_codec->simulcastStream[0].numberOfTemporalLayers = 0;
    video_codec->simulcastStream[0].maxBitrate = 150;
    video_codec->simulcastStream[0].targetBitrate = 150;
    video_codec->simulcastStream[0].minBitrate = 50;
    video_codec->simulcastStream[0].qpMax = video_codec->qpMax;

    video_codec->simulcastStream[1].width = 640;
    video_codec->simulcastStream[1].height = 360;
    video_codec->simulcastStream[1].numberOfTemporalLayers = 0;
    video_codec->simulcastStream[1].maxBitrate = 500;
    video_codec->simulcastStream[1].targetBitrate = 500;
    video_codec->simulcastStream[1].minBitrate = 150;
    video_codec->simulcastStream[1].qpMax = video_codec->qpMax;

    video_codec->simulcastStream[2].width = 1280;
    video_codec->simulcastStream[2].height = 720;
    video_codec->simulcastStream[2].numberOfTemporalLayers = 0;
    video_codec->simulcastStream[2].maxBitrate = 1200;
    video_codec->simulcastStream[2].targetBitrate = 1200;
    video_codec->simulcastStream[2].minBitrate = 600;
    video_codec->simulcastStream[2].qpMax = video_codec->qpMax;
  }

 protected:
  std::map<uint32_t, bool> reserved_ssrcs_;
};

TEST_P(RampUpTest, RampUpWithPadding) {
  test::DirectTransport receiver_transport;
  StreamObserver stream_observer(3, &receiver_transport,
                                 Clock::GetRealTimeClock());
  newapi::VideoCall::Config call_config(&stream_observer);
  scoped_ptr<newapi::VideoCall> call(newapi::VideoCall::Create(call_config));
  newapi::VideoSendStream::Config send_config =
      call->GetDefaultSendConfig();

  receiver_transport.SetReceiver(call->Receiver());

  FakeEncoder encoder(Clock::GetRealTimeClock());
  send_config.encoder = &encoder;
  send_config.internal_source = false;
  SetCodecStreamSettings(&send_config.codec);
  send_config.codec.plType = 100;
  send_config.pacing = GetParam();

  test::GenerateRandomSsrcs(&send_config, &reserved_ssrcs_);

  newapi::VideoSendStream* send_stream =
      call->CreateSendStream(send_config);

  newapi::VideoReceiveStream::Config receive_config;
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.nack.rtp_history_ms =
      send_config.rtp.nack.rtp_history_ms;
  newapi::VideoReceiveStream* receive_stream = call->CreateReceiveStream(
      receive_config);

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream->Input(),
          test::FrameGenerator::Create(
              send_config.codec.width, send_config.codec.height,
              Clock::GetRealTimeClock()),
          30));

  receive_stream->StartReceive();
  send_stream->StartSend();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, stream_observer.Wait());

  frame_generator_capturer->Stop();
  send_stream->StopSend();
  receive_stream->StopReceive();

  call->DestroyReceiveStream(receive_stream);
  call->DestroySendStream(send_stream);
}

INSTANTIATE_TEST_CASE_P(RampUpTest, RampUpTest, ::testing::Bool());

struct EngineTestParams {
  size_t width, height;
  struct {
    unsigned int min, start, max;
  } bitrate;
};

class EngineTest : public ::testing::TestWithParam<EngineTestParams> {
 public:
  EngineTest() : send_stream_(NULL), receive_stream_(NULL) {}

  ~EngineTest() {
    EXPECT_EQ(NULL, send_stream_);
    EXPECT_EQ(NULL, receive_stream_);
  }

 protected:
  void CreateCalls(newapi::Transport* sender_transport,
                   newapi::Transport* receiver_transport) {
    newapi::VideoCall::Config sender_config(sender_transport);
    newapi::VideoCall::Config receiver_config(receiver_transport);
    sender_call_.reset(newapi::VideoCall::Create(sender_config));
    receiver_call_.reset(newapi::VideoCall::Create(receiver_config));
  }

  void CreateTestConfigs() {
    EngineTestParams params = GetParam();
    send_config_ = sender_call_->GetDefaultSendConfig();
    receive_config_ = receiver_call_->GetDefaultReceiveConfig();

    test::GenerateRandomSsrcs(&send_config_, &reserved_ssrcs_);
    send_config_.codec.width = static_cast<uint16_t>(params.width);
    send_config_.codec.height = static_cast<uint16_t>(params.height);
    send_config_.codec.minBitrate = params.bitrate.min;
    send_config_.codec.startBitrate = params.bitrate.start;
    send_config_.codec.maxBitrate = params.bitrate.max;

    receive_config_.rtp.ssrc = send_config_.rtp.ssrcs[0];
  }

  void CreateStreams() {
    assert(send_stream_ == NULL);
    assert(receive_stream_ == NULL);

    send_stream_ = sender_call_->CreateSendStream(send_config_);
    receive_stream_ = receiver_call_->CreateReceiveStream(receive_config_);
  }

  void CreateFrameGenerator() {
    EngineTestParams params = GetParam();
    frame_generator_capturer_.reset(test::FrameGeneratorCapturer::Create(
        send_stream_->Input(),
        test::FrameGenerator::Create(
            params.width, params.height, Clock::GetRealTimeClock()),
        30));
  }

  void StartSending() {
    receive_stream_->StartReceive();
    send_stream_->StartSend();
    frame_generator_capturer_->Start();
  }

  void StopSending() {
    frame_generator_capturer_->Stop();
    send_stream_->StopSend();
    receive_stream_->StopReceive();
  }

  void DestroyStreams() {
    sender_call_->DestroySendStream(send_stream_);
    receiver_call_->DestroyReceiveStream(receive_stream_);
    send_stream_= NULL;
    receive_stream_ = NULL;
  }

  void ReceivesPliAndRecovers(int rtp_history_ms);

  scoped_ptr<newapi::VideoCall> sender_call_;
  scoped_ptr<newapi::VideoCall> receiver_call_;

  newapi::VideoSendStream::Config send_config_;
  newapi::VideoReceiveStream::Config receive_config_;

  newapi::VideoSendStream* send_stream_;
  newapi::VideoReceiveStream* receive_stream_;

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;

  std::map<uint32_t, bool> reserved_ssrcs_;
};

// TODO(pbos): What are sane values here for bitrate? Are we missing any
// important resolutions?
EngineTestParams video_1080p = {1920, 1080, {300, 600, 800}};
EngineTestParams video_720p = {1280, 720, {300, 600, 800}};
EngineTestParams video_vga = {640, 480, {300, 600, 800}};
EngineTestParams video_qvga = {320, 240, {300, 600, 800}};
EngineTestParams video_4cif = {704, 576, {300, 600, 800}};
EngineTestParams video_cif = {352, 288, {300, 600, 800}};
EngineTestParams video_qcif = {176, 144, {300, 600, 800}};

class NackObserver : public test::RtpRtcpObserver {
  static const int kNumberOfNacksToObserve = 4;
  static const int kInverseProbabilityToStartLossBurst = 20;
  static const int kMaxLossBurst = 10;
 public:
  NackObserver()
      : received_all_retransmissions_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        drop_burst_count_(0),
        sent_rtp_packets_(0),
        nacks_left_(kNumberOfNacksToObserve) {}

  EventTypeWrapper Wait() {
    // 2 minutes should be more than enough time for the test to finish.
    return received_all_retransmissions_->Wait(2 * 60 * 1000);
  }

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
      received_all_retransmissions_->Set();
    }
  }

  scoped_ptr<EventWrapper> received_all_retransmissions_;

  scoped_ptr<RtpHeaderParser> rtp_parser_;
  std::set<uint16_t> dropped_packets_;
  std::set<uint16_t> retransmitted_packets_;
  int drop_burst_count_;
  uint64_t sent_rtp_packets_;
  int nacks_left_;
  int rtcp_without_nack_count_;
  static const int kRequiredRtcpsWithoutNack = 2;
};

TEST_P(EngineTest, ReceivesAndRetransmitsNack) {
  NackObserver observer;

  CreateCalls(observer.SendTransport(), observer.ReceiveTransport());

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

  DestroyStreams();

  observer.StopSending();
}

class PliObserver : public test::RtpRtcpObserver {
  static const int kInverseDropProbability = 16;
 public:
  PliObserver(bool nack_enabled) :
        renderer_(this),
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

  class ReceiverRenderer : public newapi::VideoRenderer {
   public:
    ReceiverRenderer(PliObserver* observer)
        : rendered_retransmission_(EventWrapper::Create()),
          observer_(observer) {}

    virtual void RenderFrame(const I420VideoFrame& video_frame,
                             int time_to_render_ms) {
      CriticalSectionScoped crit_(observer_->lock_.get());
      if (observer_->first_retransmitted_timestamp_ != 0 &&
          video_frame.timestamp() > observer_->first_retransmitted_timestamp_) {
        EXPECT_TRUE(observer_->received_pli_);
        rendered_retransmission_->Set();
      }
      observer_->rendered_frame_ = true;
    }
    scoped_ptr<EventWrapper> rendered_retransmission_;
    PliObserver* observer_;
  } renderer_;

  EventTypeWrapper Wait() {
    // 120 seconds should be plenty of time.
    return renderer_.rendered_retransmission_->Wait(2 * 60 * 1000);
  }

 private:
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  bool nack_enabled_;

  uint32_t first_retransmitted_timestamp_;
  uint32_t last_send_timestamp_;

  bool rendered_frame_;
  bool received_pli_;
};

void EngineTest::ReceivesPliAndRecovers(int rtp_history_ms) {
  PliObserver observer(rtp_history_ms > 0);

  CreateCalls(observer.SendTransport(), observer.ReceiveTransport());

  observer.SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());

  CreateTestConfigs();
  send_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.rtp.nack.rtp_history_ms = rtp_history_ms;
  receive_config_.renderer = &observer.renderer_;

  CreateStreams();
  CreateFrameGenerator();

  StartSending();

  // Wait() waits for an event triggered when Pli has been received and frames
  // have been rendered afterwards.
  EXPECT_EQ(kEventSignaled, observer.Wait());

  StopSending();

  DestroyStreams();

  observer.StopSending();
}

TEST_P(EngineTest, ReceivesPliAndRecoversWithNack) {
  ReceivesPliAndRecovers(1000);
}

// TODO(pbos): Enable this when 2250 is resolved.
TEST_P(EngineTest, DISABLED_ReceivesPliAndRecoversWithoutNack) {
  ReceivesPliAndRecovers(0);
}

INSTANTIATE_TEST_CASE_P(EngineTest, EngineTest, ::testing::Values(video_vga));
}  // namespace webrtc
