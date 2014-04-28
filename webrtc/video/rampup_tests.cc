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
#include "webrtc/test/encoder_settings.h"
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
  StreamObserver(const SsrcMap& rtx_media_ssrcs,
                 newapi::Transport* feedback_transport,
                 Clock* clock)
      : clock_(clock),
        test_done_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(feedback_transport),
        receive_stats_(ReceiveStatistics::Create(clock)),
        payload_registry_(
            new RTPPayloadRegistry(RTPPayloadStrategy::CreateStrategy(false))),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        expected_bitrate_bps_(0),
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
        rbe_factory.Create(this, clock, kMimdControl,
                           kRemoteBitrateEstimatorMinBitrateBps));
  }

  void set_expected_bitrate_bps(unsigned int expected_bitrate_bps) {
    CriticalSectionScoped lock(crit_.get());
    expected_bitrate_bps_ = expected_bitrate_bps;
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) OVERRIDE {
    CriticalSectionScoped lock(crit_.get());
    assert(expected_bitrate_bps_ > 0);
    if (bitrate >= expected_bitrate_bps_) {
      // Just trigger if there was any rtx padding packet.
      if (rtx_media_ssrcs_.empty() || rtx_media_sent_ > 0) {
        TriggerTestDone();
      }
    }
    rtp_rtcp_->SetREMBData(
        bitrate, static_cast<uint8_t>(ssrcs.size()), &ssrcs[0]);
    rtp_rtcp_->Process();
  }

  virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE {
    CriticalSectionScoped lock(crit_.get());
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

  EventTypeWrapper Wait() { return test_done_->Wait(120 * 1000); }

 private:
  void ReportResult(const std::string& measurement,
                    size_t value,
                    const std::string& units) {
    webrtc::test::PrintResult(
        measurement, "",
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
        value, units, false);
  }

  void TriggerTestDone() EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    ReportResult("total-sent", total_sent_, "bytes");
    ReportResult("padding-sent", padding_sent_, "bytes");
    ReportResult("rtx-media-sent", rtx_media_sent_, "bytes");
    ReportResult("total-packets-sent", total_packets_sent_, "packets");
    ReportResult("padding-packets-sent", padding_packets_sent_, "packets");
    ReportResult("rtx-packets-sent", rtx_media_packets_sent_, "packets");
    test_done_->Set();
  }

  Clock* const clock_;
  const scoped_ptr<EventWrapper> test_done_;
  const scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  const scoped_ptr<ReceiveStatistics> receive_stats_;
  const scoped_ptr<RTPPayloadRegistry> payload_registry_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;

  const scoped_ptr<CriticalSectionWrapper> crit_;
  unsigned int expected_bitrate_bps_ GUARDED_BY(crit_);
  SsrcMap rtx_media_ssrcs_ GUARDED_BY(crit_);
  size_t total_sent_ GUARDED_BY(crit_);
  size_t padding_sent_ GUARDED_BY(crit_);
  size_t rtx_media_sent_ GUARDED_BY(crit_);
  int total_packets_sent_ GUARDED_BY(crit_);
  int padding_packets_sent_ GUARDED_BY(crit_);
  int rtx_media_packets_sent_ GUARDED_BY(crit_);
};

class LowRateStreamObserver : public test::DirectTransport,
                              public RemoteBitrateObserver,
                              public PacketReceiver {
 public:
  LowRateStreamObserver(newapi::Transport* feedback_transport,
                        Clock* clock,
                        size_t number_of_streams,
                        bool rtx_used)
      : clock_(clock),
        number_of_streams_(number_of_streams),
        rtx_used_(rtx_used),
        test_done_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        feedback_transport_(feedback_transport),
        receive_stats_(ReceiveStatistics::Create(clock)),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        send_stream_(NULL),
        test_state_(kFirstRampup),
        state_start_ms_(clock_->TimeInMilliseconds()),
        interval_start_ms_(state_start_ms_),
        last_remb_bps_(0),
        sent_bytes_(0),
        total_overuse_bytes_(0),
        suspended_in_stats_(false) {
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
        rbe_factory.Create(this, clock, kMimdControl,
                           kRemoteBitrateEstimatorMinBitrateBps));
    forward_transport_config_.link_capacity_kbps =
        kHighBandwidthLimitBps / 1000;
    forward_transport_config_.queue_length = 100;  // Something large.
    test::DirectTransport::SetConfig(forward_transport_config_);
    test::DirectTransport::SetReceiver(this);
  }

  virtual void SetSendStream(const VideoSendStream* send_stream) {
    CriticalSectionScoped lock(crit_.get());
    send_stream_ = send_stream;
  }

  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    CriticalSectionScoped lock(crit_.get());
    rtp_rtcp_->SetREMBData(
        bitrate, static_cast<uint8_t>(ssrcs.size()), &ssrcs[0]);
    rtp_rtcp_->Process();
    last_remb_bps_ = bitrate;
  }

  virtual bool SendRtp(const uint8_t* data, size_t length) OVERRIDE {
    CriticalSectionScoped lock(crit_.get());
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
    CriticalSectionScoped lock(crit_.get());
    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));
    receive_stats_->IncomingPacket(header, length, false);
    remote_bitrate_estimator_->IncomingPacket(
        clock_->TimeInMilliseconds(), static_cast<int>(length - 12), header);
    if (remote_bitrate_estimator_->TimeUntilNextProcess() <= 0) {
      remote_bitrate_estimator_->Process();
    }
    suspended_in_stats_ = send_stream_->GetStats().suspended;
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
    sprintf(temp_str, "%i", static_cast<int>(number_of_streams_));
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
    CriticalSectionScoped lock(crit_.get());
    assert(send_stream_ != NULL);
    switch (test_state_) {
      case kFirstRampup: {
        EXPECT_FALSE(suspended_in_stats_);
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
        if (bitrate_bps < kExpectedLowBitrateBps && suspended_in_stats_) {
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
        if (bitrate_bps > kExpectedHighBitrateBps && !suspended_in_stats_) {
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

  Clock* const clock_;
  const size_t number_of_streams_;
  const bool rtx_used_;
  const scoped_ptr<EventWrapper> test_done_;
  const scoped_ptr<RtpHeaderParser> rtp_parser_;
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  internal::TransportAdapter feedback_transport_;
  const scoped_ptr<ReceiveStatistics> receive_stats_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;

  scoped_ptr<CriticalSectionWrapper> crit_;
  const VideoSendStream* send_stream_ GUARDED_BY(crit_);
  FakeNetworkPipe::Config forward_transport_config_ GUARDED_BY(crit_);
  TestStates test_state_ GUARDED_BY(crit_);
  int64_t state_start_ms_ GUARDED_BY(crit_);
  int64_t interval_start_ms_ GUARDED_BY(crit_);
  unsigned int last_remb_bps_ GUARDED_BY(crit_);
  size_t sent_bytes_ GUARDED_BY(crit_);
  size_t total_overuse_bytes_ GUARDED_BY(crit_);
  bool suspended_in_stats_ GUARDED_BY(crit_);
};
}

class RampUpTest : public ::testing::Test {
 public:
  virtual void SetUp() { reserved_ssrcs_.clear(); }

 protected:
  void RunRampUpTest(bool pacing, bool rtx, size_t num_streams) {
    std::vector<uint32_t> ssrcs(GenerateSsrcs(num_streams, 100));
    std::vector<uint32_t> rtx_ssrcs(GenerateSsrcs(num_streams, 200));
    StreamObserver::SsrcMap rtx_ssrc_map;
    if (rtx) {
      for (size_t i = 0; i < ssrcs.size(); ++i)
        rtx_ssrc_map[rtx_ssrcs[i]] = ssrcs[i];
    }
    test::DirectTransport receiver_transport;
    StreamObserver stream_observer(rtx_ssrc_map,
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
    send_config.encoder_settings =
        test::CreateEncoderSettings(&encoder, "FAKE", 125, num_streams);

    if (num_streams == 1) {
      send_config.encoder_settings.streams[0].target_bitrate_bps = 2000000;
      send_config.encoder_settings.streams[0].max_bitrate_bps = 2000000;
    }

    send_config.pacing = pacing;
    send_config.rtp.nack.rtp_history_ms = 1000;
    send_config.rtp.ssrcs = ssrcs;
    if (rtx) {
      send_config.rtp.rtx.payload_type = 96;
      send_config.rtp.rtx.ssrcs = rtx_ssrcs;
    }
    send_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsoluteSendTimeExtensionId));

    if (num_streams == 1) {
      // For single stream rampup until 1mbps
      stream_observer.set_expected_bitrate_bps(1000000);
    } else {
      // For multi stream rampup until all streams are being sent. That means
      // enough birate to sent all the target streams plus the min bitrate of
      // the last one.
      int expected_bitrate_bps =
          send_config.encoder_settings.streams.back().min_bitrate_bps;
      for (size_t i = 0; i < send_config.encoder_settings.streams.size() - 1;
           ++i) {
        expected_bitrate_bps +=
            send_config.encoder_settings.streams[i].target_bitrate_bps;
      }
      stream_observer.set_expected_bitrate_bps(expected_bitrate_bps);
    }

    VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);

    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
        test::FrameGeneratorCapturer::Create(
            send_stream->Input(),
            send_config.encoder_settings.streams.back().width,
            send_config.encoder_settings.streams.back().height,
            send_config.encoder_settings.streams.back().max_framerate,
            Clock::GetRealTimeClock()));

    send_stream->Start();
    frame_generator_capturer->Start();

    EXPECT_EQ(kEventSignaled, stream_observer.Wait());

    frame_generator_capturer->Stop();
    send_stream->Stop();

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
    send_config.encoder_settings =
        test::CreateEncoderSettings(&encoder, "FAKE", 125, number_of_streams);
    send_config.rtp.nack.rtp_history_ms = 1000;
    send_config.rtp.ssrcs.insert(
        send_config.rtp.ssrcs.begin(), ssrcs.begin(), ssrcs.end());
    send_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsoluteSendTimeExtensionId));
    send_config.suspend_below_min_bitrate = true;

    VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);
    stream_observer.SetSendStream(send_stream);

    size_t width = 0;
    size_t height = 0;
    for (size_t i = 0; i < send_config.encoder_settings.streams.size(); ++i) {
      size_t stream_width = send_config.encoder_settings.streams[i].width;
      size_t stream_height = send_config.encoder_settings.streams[i].height;
      if (stream_width > width)
        width = stream_width;
      if (stream_height > height)
        height = stream_height;
    }

    scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
        test::FrameGeneratorCapturer::Create(send_stream->Input(),
                                             width,
                                             height,
                                             30,
                                             Clock::GetRealTimeClock()));

    send_stream->Start();
    frame_generator_capturer->Start();

    EXPECT_EQ(kEventSignaled, stream_observer.Wait());

    stream_observer.StopSending();
    receiver_transport.StopSending();
    frame_generator_capturer->Stop();
    send_stream->Stop();

    call->DestroyVideoSendStream(send_stream);
  }

 private:
  std::vector<uint32_t> GenerateSsrcs(size_t num_streams,
                                      uint32_t ssrc_offset) {
    std::vector<uint32_t> ssrcs;
    for (size_t i = 0; i != num_streams; ++i)
      ssrcs.push_back(static_cast<uint32_t>(ssrc_offset + i));
    return ssrcs;
  }

  std::map<uint32_t, bool> reserved_ssrcs_;
};

TEST_F(RampUpTest, SingleStreamWithoutPacing) {
  RunRampUpTest(false, false, 1);
}

TEST_F(RampUpTest, SingleStreamWithPacing) {
  RunRampUpTest(true, false, 1);
}

TEST_F(RampUpTest, SimulcastWithoutPacing) {
  RunRampUpTest(false, false, 3);
}

TEST_F(RampUpTest, SimulcastWithPacing) {
  RunRampUpTest(true, false, 3);
}

// TODO(pbos): Re-enable, webrtc:2992.
TEST_F(RampUpTest, DISABLED_SimulcastWithPacingAndRtx) {
  RunRampUpTest(true, true, 3);
}

TEST_F(RampUpTest, UpDownUpOneStream) { RunRampUpDownUpTest(1, false); }

TEST_F(RampUpTest, UpDownUpThreeStreams) { RunRampUpDownUpTest(3, false); }

TEST_F(RampUpTest, UpDownUpOneStreamRtx) { RunRampUpDownUpTest(1, true); }

TEST_F(RampUpTest, UpDownUpThreeStreamsRtx) { RunRampUpDownUpTest(3, true); }

}  // namespace webrtc
