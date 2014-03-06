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
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/experiments.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"

namespace webrtc {

namespace {
static const int kAbsoluteSendTimeExtensionId = 7;
static const int kMaxPacketSize = 1500;

class StreamObserver : public newapi::Transport, public RemoteBitrateObserver {
 public:
  typedef std::map<uint32_t, int> BytesSentMap;
  typedef std::map<uint32_t, uint32_t> SsrcMap;
  StreamObserver(int num_expected_ssrcs,
                 const SsrcMap& rtx_media_ssrcs,
                 newapi::Transport* feedback_transport,
                 Clock* clock)
      : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
        all_ssrcs_sent_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(feedback_transport),
        receive_stats_(ReceiveStatistics::Create(clock)),
        payload_registry_(
            new RTPPayloadRegistry(-1,
                                   RTPPayloadStrategy::CreateStrategy(false))),
        clock_(clock),
        num_expected_ssrcs_(num_expected_ssrcs),
        rtx_media_ssrcs_(rtx_media_ssrcs),
        total_sent_(0),
        padding_sent_(0),
        rtx_media_sent_(0),
        total_packets_sent_(0),
        padding_packets_sent_(0),
        rtx_media_packets_sent_(0) {
    // Ideally we would only have to instantiate an RtcpSender, an
    // RtpHeaderParser and a RemoteBitrateEstimator here, but due to the current
    // state of the RTP module we need a full module and receive statistics to
    // be able to produce an RTCP with REMB.
    RtpRtcp::Configuration config;
    config.receive_statistics = receive_stats_.get();
    feedback_transport_.Enable();
    config.outgoing_transport = &feedback_transport_;
    rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
    rtp_rtcp_->SetREMBStatus(true);
    rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                            kAbsoluteSendTimeExtensionId);
    AbsoluteSendTimeRemoteBitrateEstimatorFactory rbe_factory;
    const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;
    remote_bitrate_estimator_.reset(
        rbe_factory.Create(this, clock, kRemoteBitrateEstimatorMinBitrateBps));
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    CriticalSectionScoped lock(critical_section_.get());
    if (ssrcs.size() == num_expected_ssrcs_ && bitrate >= kExpectedBitrateBps) {
      if (rtx_media_ssrcs_.empty() || rtx_media_sent_ > 0) {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        webrtc::test::PrintResult(
            "total-sent", "", test_info->name(), total_sent_, "bytes", false);
        webrtc::test::PrintResult("padding-sent",
                                  "",
                                  test_info->name(),
                                  padding_sent_,
                                  "bytes",
                                  false);
        webrtc::test::PrintResult("rtx-media-sent",
                                  "",
                                  test_info->name(),
                                  rtx_media_sent_,
                                  "bytes",
                                  false);
        webrtc::test::PrintResult("total-packets-sent",
                                  "",
                                  test_info->name(),
                                  total_packets_sent_,
                                  "packets",
                                  false);
        webrtc::test::PrintResult("padding-packets-sent",
                                  "",
                                  test_info->name(),
                                  padding_packets_sent_,
                                  "packets",
                                  false);
        webrtc::test::PrintResult("rtx-packets-sent",
                                  "",
                                  test_info->name(),
                                  rtx_media_packets_sent_,
                                  "packets",
                                  false);
        all_ssrcs_sent_->Set();
      }
    }
    rtp_rtcp_->SetREMBData(
        bitrate, static_cast<uint8_t>(ssrcs.size()), &ssrcs[0]);
    rtp_rtcp_->Process();
  }

  virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    CriticalSectionScoped lock(critical_section_.get());
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));
    receive_stats_->IncomingPacket(header, length, false);
    payload_registry_->SetIncomingPayloadType(header);
    remote_bitrate_estimator_->IncomingPacket(
        clock_->TimeInMilliseconds(), static_cast<int>(length - 12), header);
    if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
      remote_bitrate_estimator_->Process();
    }
    total_sent_ += length;
    padding_sent_ += header.paddingLength;
    ++total_packets_sent_;
    if (header.paddingLength > 0)
      ++padding_packets_sent_;
    if (rtx_media_ssrcs_.find(header.ssrc) != rtx_media_ssrcs_.end()) {
      rtx_media_sent_ += length - header.headerLength - header.paddingLength;
      if (header.paddingLength == 0)
        ++rtx_media_packets_sent_;
      uint8_t restored_packet[kMaxPacketSize];
      uint8_t* restored_packet_ptr = restored_packet;
      int restored_length = static_cast<int>(length);
      payload_registry_->RestoreOriginalPacket(&restored_packet_ptr,
                                               packet,
                                               &restored_length,
                                               rtx_media_ssrcs_[header.ssrc],
                                               header);
      length = restored_length;
      EXPECT_TRUE(rtp_parser_->Parse(
          restored_packet, static_cast<int>(length), &header));
    } else {
      rtp_rtcp_->SetRemoteSSRC(header.ssrc);
    }
    return true;
  }

  virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    return true;
  }

  EventTypeWrapper Wait() { return all_ssrcs_sent_->Wait(120 * 1000); }

 private:
  static const unsigned int kExpectedBitrateBps = 1200000;

  scoped_ptr<CriticalSectionWrapper> critical_section_;
  scoped_ptr<EventWrapper> all_ssrcs_sent_;
  scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RTPPayloadRegistry> payload_registry_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  Clock* clock_;
  const size_t num_expected_ssrcs_;
  SsrcMap rtx_media_ssrcs_;
  size_t total_sent_;
  size_t padding_sent_;
  size_t rtx_media_sent_;
  int total_packets_sent_;
  int padding_packets_sent_;
  int rtx_media_packets_sent_;
};

class LowRateStreamObserver : public test::DirectTransport,
                              public RemoteBitrateObserver,
                              public PacketReceiver {
 public:
  LowRateStreamObserver(newapi::Transport* feedback_transport,
                        Clock* clock,
                        size_t number_of_streams,
                        bool rtx_used)
      : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
        test_done_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(feedback_transport),
        receive_stats_(ReceiveStatistics::Create(clock)),
        clock_(clock),
        test_state_(kFirstRampup),
        state_start_ms_(clock_->TimeInMilliseconds()),
        interval_start_ms_(state_start_ms_),
        last_remb_bps_(0),
        sent_bytes_(0),
        total_overuse_bytes_(0),
        number_of_streams_(number_of_streams),
        rtx_used_(rtx_used) {
    RtpRtcp::Configuration config;
    config.receive_statistics = receive_stats_.get();
    feedback_transport_.Enable();
    config.outgoing_transport = &feedback_transport_;
    rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(config));
    rtp_rtcp_->SetREMBStatus(true);
    rtp_rtcp_->SetRTCPStatus(kRtcpNonCompound);
    rtp_parser_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                            kAbsoluteSendTimeExtensionId);
    AbsoluteSendTimeRemoteBitrateEstimatorFactory rbe_factory;
    const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 10000;
    remote_bitrate_estimator_.reset(
        rbe_factory.Create(this, clock, kRemoteBitrateEstimatorMinBitrateBps));
    forward_transport_config_.link_capacity_kbps =
        kHighBandwidthLimitBps / 1000;
    forward_transport_config_.queue_length = 100;  // Something large.
    test::DirectTransport::SetConfig(forward_transport_config_);
    test::DirectTransport::SetReceiver(this);
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    CriticalSectionScoped lock(critical_section_.get());
    rtp_rtcp_->SetREMBData(
        bitrate, static_cast<uint8_t>(ssrcs.size()), &ssrcs[0]);
    rtp_rtcp_->Process();
    last_remb_bps_ = bitrate;
  }

  virtual bool SendRtp(const uint8_t* data, size_t length) OVERRIDE {
    sent_bytes_ += length;
    int64_t now_ms = clock_->TimeInMilliseconds();
    if (now_ms > interval_start_ms_ + 1000) {  // Let at least 1 second pass.
      // Verify that the send rate was about right.
      unsigned int average_rate_bps = static_cast<unsigned int>(sent_bytes_) *
                                      8 * 1000 / (now_ms - interval_start_ms_);
      // TODO(holmer): Why is this failing?
      // EXPECT_LT(average_rate_bps, last_remb_bps_ * 1.1);
      if (average_rate_bps > last_remb_bps_ * 1.1) {
        total_overuse_bytes_ +=
            sent_bytes_ -
            last_remb_bps_ / 8 * (now_ms - interval_start_ms_) / 1000;
      }
      EvolveTestState(average_rate_bps);
      interval_start_ms_ = now_ms;
      sent_bytes_ = 0;
    }
    return test::DirectTransport::SendRtp(data, length);
  }

  virtual bool DeliverPacket(const uint8_t* packet, size_t length) OVERRIDE {
    CriticalSectionScoped lock(critical_section_.get());
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));
    receive_stats_->IncomingPacket(header, length, false);
    remote_bitrate_estimator_->IncomingPacket(
        clock_->TimeInMilliseconds(), static_cast<int>(length - 12), header);
    if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
      remote_bitrate_estimator_->Process();
    }
    return true;
  }

  virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    return true;
  }

  // Produces a string similar to "1stream_nortx", depending on the values of
  // number_of_streams_ and rtx_used_;
  std::string GetModifierString() {
    std::string str("_");
    char temp_str[5];
    sprintf(temp_str, "%zu", number_of_streams_);
    str += std::string(temp_str);
    str += "stream";
    str += (number_of_streams_ > 1 ? "s" : "");
    str += "_";
    str += (rtx_used_ ? "" : "no");
    str += "rtx";
    return str;
  }

  // This method defines the state machine for the ramp up-down-up test.
  void EvolveTestState(unsigned int bitrate_bps) {
    int64_t now = clock_->TimeInMilliseconds();
    switch (test_state_) {
      case kFirstRampup: {
        if (bitrate_bps > kExpectedHighBitrateBps) {
          // The first ramp-up has reached the target bitrate. Change the
          // channel limit, and move to the next test state.
          forward_transport_config_.link_capacity_kbps =
              kLowBandwidthLimitBps / 1000;
          test::DirectTransport::SetConfig(forward_transport_config_);
          test_state_ = kLowRate;
          webrtc::test::PrintResult("ramp_up_down_up",
                                    GetModifierString(),
                                    "first_rampup",
                                    now - state_start_ms_,
                                    "ms",
                                    false);
          state_start_ms_ = now;
          interval_start_ms_ = now;
          sent_bytes_ = 0;
        }
        break;
      }
      case kLowRate: {
        if (bitrate_bps < kExpectedLowBitrateBps) {
          // The ramp-down was successful. Change the channel limit back to a
          // high value, and move to the next test state.
          forward_transport_config_.link_capacity_kbps =
              kHighBandwidthLimitBps / 1000;
          test::DirectTransport::SetConfig(forward_transport_config_);
          test_state_ = kSecondRampup;
          webrtc::test::PrintResult("ramp_up_down_up",
                                    GetModifierString(),
                                    "rampdown",
                                    now - state_start_ms_,
                                    "ms",
                                    false);
          state_start_ms_ = now;
          interval_start_ms_ = now;
          sent_bytes_ = 0;
        }
        break;
      }
      case kSecondRampup: {
        if (bitrate_bps > kExpectedHighBitrateBps) {
          webrtc::test::PrintResult("ramp_up_down_up",
                                    GetModifierString(),
                                    "second_rampup",
                                    now - state_start_ms_,
                                    "ms",
                                    false);
          webrtc::test::PrintResult("ramp_up_down_up",
                                    GetModifierString(),
                                    "total_overuse",
                                    total_overuse_bytes_,
                                    "bytes",
                                    false);
          test_done_->Set();
        }
        break;
      }
    }
  }

  EventTypeWrapper Wait() { return test_done_->Wait(120 * 1000); }

 private:
  static const unsigned int kHighBandwidthLimitBps = 80000;
  static const unsigned int kExpectedHighBitrateBps = 60000;
  static const unsigned int kLowBandwidthLimitBps = 20000;
  static const unsigned int kExpectedLowBitrateBps = 20000;
  enum TestStates { kFirstRampup, kLowRate, kSecondRampup };

  scoped_ptr<CriticalSectionWrapper> critical_section_;
  scoped_ptr<EventWrapper> test_done_;
  scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  Clock* clock_;
  FakeNetworkPipe::Config forward_transport_config_;
  TestStates test_state_;
  int64_t state_start_ms_;
  int64_t interval_start_ms_;
  unsigned int last_remb_bps_;
  size_t sent_bytes_;
  size_t total_overuse_bytes_;
  const size_t number_of_streams_;
  const bool rtx_used_;
};
}

class RampUpTest : public ::testing::Test {
 public:
  virtual void SetUp() { reserved_ssrcs_.clear(); }

 protected:
  void RunRampUpTest(bool pacing, bool rtx) {
    const size_t kNumberOfStreams = 3;
    std::vector<uint32_t> ssrcs;
    for (size_t i = 0; i < kNumberOfStreams; ++i)
      ssrcs.push_back(static_cast<uint32_t>(i + 1));
    uint32_t kRtxSsrcs[kNumberOfStreams] = {111, 112, 113};
    StreamObserver::SsrcMap rtx_ssrc_map;
    if (rtx) {
      for (size_t i = 0; i < ssrcs.size(); ++i)
        rtx_ssrc_map[kRtxSsrcs[i]] = ssrcs[i];
    }
    test::DirectTransport receiver_transport;
    int num_expected_ssrcs = kNumberOfStreams + (rtx ? 1 : 0);
    StreamObserver stream_observer(num_expected_ssrcs,
                                   rtx_ssrc_map,
                                   &receiver_transport,
                                   Clock::GetRealTimeClock());

    Call::Config call_config(&stream_observer);
    webrtc::Config webrtc_config;
    call_config.webrtc_config = &webrtc_config;
    webrtc_config.Set<PaddingStrategy>(new PaddingStrategy(rtx));
    scoped_ptr<Call> call(Call::Create(call_config));
    VideoSendStream::Config send_config = call->GetDefaultSendConfig();

    receiver_transport.SetReceiver(call->Receiver());

    test::FakeEncoder encoder(Clock::GetRealTimeClock());
    send_config.encoder = &encoder;
    send_config.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config.codec, kNumberOfStreams);
    send_config.codec.plType = 125;
    send_config.pacing = pacing;
    send_config.rtp.nack.rtp_history_ms = 1000;
    send_config.rtp.ssrcs.insert(
        send_config.rtp.ssrcs.begin(), ssrcs.begin(), ssrcs.end());
    if (rtx) {
      send_config.rtp.rtx.payload_type = 96;
      send_config.rtp.rtx.ssrcs.insert(send_config.rtp.rtx.ssrcs.begin(),
                                       kRtxSsrcs,
                                       kRtxSsrcs + kNumberOfStreams);
    }
    send_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsoluteSendTimeExtensionId));

    VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);

    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
        test::FrameGeneratorCapturer::Create(send_stream->Input(),
                                             send_config.codec.width,
                                             send_config.codec.height,
                                             30,
                                             Clock::GetRealTimeClock()));

    send_stream->StartSending();
    frame_generator_capturer->Start();

    EXPECT_EQ(kEventSignaled, stream_observer.Wait());

    frame_generator_capturer->Stop();
    send_stream->StopSending();

    call->DestroyVideoSendStream(send_stream);
  }

  void RunRampUpDownUpTest(size_t number_of_streams, bool rtx) {
    std::vector<uint32_t> ssrcs;
    for (size_t i = 0; i < number_of_streams; ++i)
      ssrcs.push_back(static_cast<uint32_t>(i + 1));
    test::DirectTransport receiver_transport;
    LowRateStreamObserver stream_observer(
        &receiver_transport, Clock::GetRealTimeClock(), number_of_streams, rtx);

    Call::Config call_config(&stream_observer);
    webrtc::Config webrtc_config;
    call_config.webrtc_config = &webrtc_config;
    webrtc_config.Set<PaddingStrategy>(new PaddingStrategy(rtx));
    scoped_ptr<Call> call(Call::Create(call_config));
    VideoSendStream::Config send_config = call->GetDefaultSendConfig();

    receiver_transport.SetReceiver(call->Receiver());

    test::FakeEncoder encoder(Clock::GetRealTimeClock());
    send_config.encoder = &encoder;
    send_config.internal_source = false;
    test::FakeEncoder::SetCodecSettings(&send_config.codec, number_of_streams);
    send_config.codec.plType = 125;
    send_config.codec.startBitrate =
        send_config.codec.simulcastStream[0].minBitrate;
    send_config.rtp.nack.rtp_history_ms = 1000;
    send_config.rtp.ssrcs.insert(
        send_config.rtp.ssrcs.begin(), ssrcs.begin(), ssrcs.end());
    send_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsoluteSendTimeExtensionId));
    send_config.suspend_below_min_bitrate = true;

    VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);

    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
        test::FrameGeneratorCapturer::Create(send_stream->Input(),
                                             send_config.codec.width,
                                             send_config.codec.height,
                                             30,
                                             Clock::GetRealTimeClock()));

    send_stream->StartSending();
    frame_generator_capturer->Start();

    EXPECT_EQ(kEventSignaled, stream_observer.Wait());

    frame_generator_capturer->Stop();
    send_stream->StopSending();

    call->DestroyVideoSendStream(send_stream);
  }

  std::map<uint32_t, bool> reserved_ssrcs_;
};

TEST_F(RampUpTest, WithoutPacing) { RunRampUpTest(false, false); }

TEST_F(RampUpTest, WithPacing) { RunRampUpTest(true, false); }

TEST_F(RampUpTest, WithPacingAndRtx) { RunRampUpTest(true, true); }

TEST_F(RampUpTest, UpDownUpOneStream) { RunRampUpDownUpTest(1, false); }

// TODO(hlundin): Find out why these tests are failing on some bots and
// re-enable.
//TEST_F(RampUpTest, UpDownUpThreeStreams) { RunRampUpDownUpTest(3, false); }
//
//TEST_F(RampUpTest, UpDownUpOneStreamRtx) { RunRampUpDownUpTest(1, true); }
//
//TEST_F(RampUpTest, UpDownUpThreeStreamsRtx) { RunRampUpDownUpTest(3, true); }

}  // namespace webrtc
