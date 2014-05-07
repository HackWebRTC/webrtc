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
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rtp_to_ntp.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/thread_annotations.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/perf_test.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

static unsigned int kLongTimeoutMs = 120 * 1000;
static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kReceiverLocalSsrc = 0x123456;
static const uint8_t kSendPayloadType = 125;

class CallPerfTest : public ::testing::Test {
 public:
  CallPerfTest()
      : send_stream_(NULL), fake_encoder_(Clock::GetRealTimeClock()) {}

 protected:
  VideoSendStream::Config GetSendTestConfig(Call* call) {
    VideoSendStream::Config config = call->GetDefaultSendConfig();
    config.rtp.ssrcs.push_back(kSendSsrc);
    config.encoder_settings = test::CreateEncoderSettings(
        &fake_encoder_, "FAKE", kSendPayloadType, 1);
    return config;
  }

  void RunVideoSendTest(Call* call,
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

  void TestMinTransmitBitrate(bool pad_to_min_bitrate);

  void TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                          int threshold_ms,
                          int start_time_ms,
                          int run_time_ms);

  VideoSendStream* send_stream_;
  test::FakeEncoder fake_encoder_;
};

class SyncRtcpObserver : public test::RtpRtcpObserver {
 public:
  explicit SyncRtcpObserver(const FakeNetworkPipe::Config& config)
      : test::RtpRtcpObserver(kLongTimeoutMs, config),
        crit_(CriticalSectionWrapper::CreateCriticalSection()) {}

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
    RTCPUtility::RTCPParserV2 parser(packet, length, true);
    EXPECT_TRUE(parser.IsValid());

    for (RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
         packet_type != RTCPUtility::kRtcpNotValidCode;
         packet_type = parser.Iterate()) {
      if (packet_type == RTCPUtility::kRtcpSrCode) {
        const RTCPUtility::RTCPPacket& packet = parser.Packet();
        RtcpMeasurement ntp_rtp_pair(
            packet.SR.NTPMostSignificant,
            packet.SR.NTPLeastSignificant,
            packet.SR.RTPTimestamp);
        StoreNtpRtpPair(ntp_rtp_pair);
      }
    }
    return SEND_PACKET;
  }

  int64_t RtpTimestampToNtp(uint32_t timestamp) const {
    CriticalSectionScoped lock(crit_.get());
    int64_t timestamp_in_ms = -1;
    if (ntp_rtp_pairs_.size() == 2) {
      // TODO(stefan): We can't EXPECT_TRUE on this call due to a bug in the
      // RTCP sender where it sends RTCP SR before any RTP packets, which leads
      // to a bogus NTP/RTP mapping.
      RtpToNtpMs(timestamp, ntp_rtp_pairs_, &timestamp_in_ms);
      return timestamp_in_ms;
    }
    return -1;
  }

 private:
  void StoreNtpRtpPair(RtcpMeasurement ntp_rtp_pair) {
    CriticalSectionScoped lock(crit_.get());
    for (RtcpList::iterator it = ntp_rtp_pairs_.begin();
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

  const scoped_ptr<CriticalSectionWrapper> crit_;
  RtcpList ntp_rtp_pairs_ GUARDED_BY(crit_);
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
      : SyncRtcpObserver(FakeNetworkPipe::Config()),
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
    webrtc::test::PrintResult("stream_offset",
                              "",
                              "synchronization",
                              ss.str(),
                              "ms",
                              false);
    int64_t time_since_creation = now_ms - creation_time_ms_;
    // During the first couple of seconds audio and video can falsely be
    // estimated as being synchronized. We don't want to trigger on those.
    if (time_since_creation < kStartupTimeMs)
      return;
    if (std::abs(latest_audio_ntp - latest_video_ntp) < kInSyncThresholdMs) {
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
  Clock* const clock_;
  int voe_channel_;
  VoEVideoSync* voe_sync_;
  SyncRtcpObserver* audio_observer_;
  int64_t creation_time_ms_;
  int64_t first_time_in_sync_;
};

TEST_F(CallPerfTest, PlaysOutAudioAndVideoInSync) {
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

  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 500;
  SyncRtcpObserver audio_observer(net_config);
  VideoRtcpAndSyncObserver observer(Clock::GetRealTimeClock(),
                                    channel,
                                    voe_sync,
                                    &audio_observer);

  Call::Config receiver_config(observer.ReceiveTransport());
  receiver_config.voice_engine = voice_engine;
  scoped_ptr<Call> sender_call(
      Call::Create(Call::Config(observer.SendTransport())));
  scoped_ptr<Call> receiver_call(Call::Create(receiver_config));
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
            channel_, packet, static_cast<unsigned int>(length), PacketTime());
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
  transport_adapter.Enable();
  EXPECT_EQ(0,
            voe_network->RegisterExternalTransport(channel, transport_adapter));

  observer.SetReceivers(receiver_call->Receiver(), sender_call->Receiver());

  test::FakeDecoder fake_decoder;

  VideoSendStream::Config send_config = GetSendTestConfig(sender_call.get());

  VideoReceiveStream::Config receive_config =
      receiver_call->GetDefaultReceiveConfig();
  assert(receive_config.codecs.empty());
  VideoCodec codec =
      test::CreateDecoderVideoCodec(send_config.encoder_settings);
  receive_config.codecs.push_back(codec);
  assert(receive_config.external_decoders.empty());
  ExternalVideoDecoder decoder;
  decoder.decoder = &fake_decoder;
  decoder.payload_type = send_config.encoder_settings.payload_type;
  receive_config.external_decoders.push_back(decoder);
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.renderer = &observer;
  receive_config.audio_channel_id = channel;

  VideoSendStream* send_stream =
      sender_call->CreateVideoSendStream(send_config);
  VideoReceiveStream* receive_stream =
      receiver_call->CreateVideoReceiveStream(receive_config);
  scoped_ptr<test::FrameGeneratorCapturer> capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream->Input(),
          send_config.encoder_settings.streams[0].width,
          send_config.encoder_settings.streams[0].height,
          30,
          Clock::GetRealTimeClock()));
  receive_stream->Start();
  send_stream->Start();
  capturer->Start();

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

  capturer->Stop();
  send_stream->Stop();
  receive_stream->Stop();
  observer.StopSending();
  audio_observer.StopSending();

  voe_base->DeleteChannel(channel);
  voe_base->Release();
  voe_codec->Release();
  voe_network->Release();
  voe_sync->Release();
  sender_call->DestroyVideoSendStream(send_stream);
  receiver_call->DestroyVideoReceiveStream(receive_stream);
  VoiceEngine::Delete(voice_engine);
}

class CaptureNtpTimeObserver : public test::RtpRtcpObserver,
                               public VideoRenderer {
 public:
  CaptureNtpTimeObserver(Clock* clock,
                         const FakeNetworkPipe::Config& config,
                         int threshold_ms,
                         int start_time_ms,
                         int run_time_ms)
      : RtpRtcpObserver(kLongTimeoutMs, config),
        clock_(clock),
        threshold_ms_(threshold_ms),
        start_time_ms_(start_time_ms),
        run_time_ms_(run_time_ms),
        creation_time_ms_(clock_->TimeInMilliseconds()),
        capturer_(NULL),
        rtp_start_timestamp_set_(false),
        rtp_start_timestamp_(0) {}

  virtual void RenderFrame(const I420VideoFrame& video_frame,
                           int time_to_render_ms) OVERRIDE {
    if (video_frame.ntp_time_ms() <= 0) {
      // Haven't got enough RTCP SR in order to calculate the capture ntp time.
      return;
    }

    int64_t now_ms = clock_->TimeInMilliseconds();
    int64_t time_since_creation = now_ms - creation_time_ms_;
    if (time_since_creation < start_time_ms_) {
      // Wait for |start_time_ms_| before start measuring.
      return;
    }

    if (time_since_creation > run_time_ms_) {
      observation_complete_->Set();
    }

    FrameCaptureTimeList::iterator iter =
        capture_time_list_.find(video_frame.timestamp());
    EXPECT_TRUE(iter != capture_time_list_.end());

    // The real capture time has been wrapped to uint32_t before converted
    // to rtp timestamp in the sender side. So here we convert the estimated
    // capture time to a uint32_t 90k timestamp also for comparing.
    uint32_t estimated_capture_timestamp =
        90 * static_cast<uint32_t>(video_frame.ntp_time_ms());
    uint32_t real_capture_timestamp = iter->second;
    int time_offset_ms = real_capture_timestamp - estimated_capture_timestamp;
    time_offset_ms = time_offset_ms / 90;
    std::stringstream ss;
    ss << time_offset_ms;

    webrtc::test::PrintResult("capture_ntp_time",
                              "",
                              "real - estimated",
                              ss.str(),
                              "ms",
                              true);
    EXPECT_TRUE(std::abs(time_offset_ms) < threshold_ms_);
  }

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, static_cast<int>(length), &header));

    if (!rtp_start_timestamp_set_) {
      // Calculate the rtp timestamp offset in order to calculate the real
      // capture time.
      uint32_t first_capture_timestamp =
          90 * static_cast<uint32_t>(capturer_->first_frame_capture_time());
      rtp_start_timestamp_ = header.timestamp - first_capture_timestamp;
      rtp_start_timestamp_set_ = true;
    }

    uint32_t capture_timestamp = header.timestamp - rtp_start_timestamp_;
    capture_time_list_.insert(capture_time_list_.end(),
                              std::make_pair(header.timestamp,
                                             capture_timestamp));
    return SEND_PACKET;
  }

  void SetCapturer(test::FrameGeneratorCapturer* capturer) {
    capturer_ = capturer;
  }

 private:
  Clock* clock_;
  int threshold_ms_;
  int start_time_ms_;
  int run_time_ms_;
  int64_t creation_time_ms_;
  test::FrameGeneratorCapturer* capturer_;
  bool rtp_start_timestamp_set_;
  uint32_t rtp_start_timestamp_;
  typedef std::map<uint32_t, uint32_t> FrameCaptureTimeList;
  FrameCaptureTimeList capture_time_list_;
};

void CallPerfTest::TestCaptureNtpTime(const FakeNetworkPipe::Config& net_config,
                                      int threshold_ms,
                                      int start_time_ms,
                                      int run_time_ms) {
  CaptureNtpTimeObserver observer(Clock::GetRealTimeClock(),
                                  net_config,
                                  threshold_ms,
                                  start_time_ms,
                                  run_time_ms);

  // Sender/receiver call.
  Call::Config receiver_config(observer.ReceiveTransport());
  scoped_ptr<Call> receiver_call(Call::Create(receiver_config));
  scoped_ptr<Call> sender_call(
      Call::Create(Call::Config(observer.SendTransport())));
  observer.SetReceivers(receiver_call->Receiver(), sender_call->Receiver());

  // Configure send stream.
  VideoSendStream::Config send_config = GetSendTestConfig(sender_call.get());
  VideoSendStream* send_stream =
      sender_call->CreateVideoSendStream(send_config);
  scoped_ptr<test::FrameGeneratorCapturer> capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream->Input(),
          send_config.encoder_settings.streams[0].width,
          send_config.encoder_settings.streams[0].height,
          30,
          Clock::GetRealTimeClock()));
  observer.SetCapturer(capturer.get());

  // Configure receive stream.
  VideoReceiveStream::Config receive_config =
      receiver_call->GetDefaultReceiveConfig();
  assert(receive_config.codecs.empty());
  VideoCodec codec =
      test::CreateDecoderVideoCodec(send_config.encoder_settings);
  receive_config.codecs.push_back(codec);
  assert(receive_config.external_decoders.empty());
  ExternalVideoDecoder decoder;
  test::FakeDecoder fake_decoder;
  decoder.decoder = &fake_decoder;
  decoder.payload_type = send_config.encoder_settings.payload_type;
  receive_config.external_decoders.push_back(decoder);
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.renderer = &observer;
  // Enable the receiver side rtt calculation.
  receive_config.rtp.rtcp_xr.receiver_reference_time_report = true;
  VideoReceiveStream* receive_stream =
      receiver_call->CreateVideoReceiveStream(receive_config);

  // Start the test
  receive_stream->Start();
  send_stream->Start();
  capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timed out while waiting for estimated capture ntp time to be "
      << "within bounds.";

  capturer->Stop();
  send_stream->Stop();
  receive_stream->Stop();
  observer.StopSending();

  sender_call->DestroyVideoSendStream(send_stream);
  receiver_call->DestroyVideoReceiveStream(receive_stream);
}

TEST_F(CallPerfTest, CaptureNtpTimeWithNetworkDelay) {
  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 100;
  // TODO(wu): lower the threshold as the calculation/estimatation becomes more
  // accurate.
  const int kThresholdMs = 30;
  const int kStartTimeMs = 10000;
  const int kRunTimeMs = 20000;
  TestCaptureNtpTime(net_config, kThresholdMs, kStartTimeMs, kRunTimeMs);
}

TEST_F(CallPerfTest, CaptureNtpTimeWithNetworkJitter) {
  FakeNetworkPipe::Config net_config;
  net_config.queue_delay_ms = 100;
  net_config.delay_standard_deviation_ms = 10;
  // TODO(wu): lower the threshold as the calculation/estimatation becomes more
  // accurate.
  const int kThresholdMs = 100;
  const int kStartTimeMs = 10000;
  const int kRunTimeMs = 20000;
  TestCaptureNtpTime(net_config, kThresholdMs, kStartTimeMs, kRunTimeMs);
}

TEST_F(CallPerfTest, RegisterCpuOveruseObserver) {
  // Verifies that either a normal or overuse callback is triggered.
  class OveruseCallbackObserver : public test::RtpRtcpObserver,
                                  public webrtc::OveruseCallback {
   public:
    OveruseCallbackObserver() : RtpRtcpObserver(kLongTimeoutMs) {}

    virtual void OnOveruse() OVERRIDE {
      observation_complete_->Set();
    }
    virtual void OnNormalUse() OVERRIDE {
      observation_complete_->Set();
    }
  };

  OveruseCallbackObserver observer;
  Call::Config call_config(observer.SendTransport());
  call_config.overuse_callback = &observer;
  scoped_ptr<Call> call(Call::Create(call_config));

  VideoSendStream::Config send_config = GetSendTestConfig(call.get());
  RunVideoSendTest(call.get(), send_config, &observer);
}

void CallPerfTest::TestMinTransmitBitrate(bool pad_to_min_bitrate) {
  static const int kMaxEncodeBitrateKbps = 30;
  static const int kMinTransmitBitrateBps = 150000;
  static const int kMinAcceptableTransmitBitrate = 130;
  static const int kMaxAcceptableTransmitBitrate = 170;
  static const int kNumBitrateObservationsInRange = 100;
  class BitrateObserver : public test::RtpRtcpObserver, public PacketReceiver {
   public:
    explicit BitrateObserver(bool using_min_transmit_bitrate)
        : test::RtpRtcpObserver(kLongTimeoutMs),
          send_stream_(NULL),
          send_transport_receiver_(NULL),
          using_min_transmit_bitrate_(using_min_transmit_bitrate),
          num_bitrate_observations_in_range_(0) {}

    virtual void SetReceivers(PacketReceiver* send_transport_receiver,
                              PacketReceiver* receive_transport_receiver)
        OVERRIDE {
      send_transport_receiver_ = send_transport_receiver;
      test::RtpRtcpObserver::SetReceivers(this, receive_transport_receiver);
    }

    void SetSendStream(VideoSendStream* send_stream) {
      send_stream_ = send_stream;
    }

   private:
    virtual bool DeliverPacket(const uint8_t* packet, size_t length) OVERRIDE {
      VideoSendStream::Stats stats = send_stream_->GetStats();
      if (stats.substreams.size() > 0) {
        assert(stats.substreams.size() == 1);
        int bitrate_kbps = stats.substreams.begin()->second.bitrate_bps / 1000;
        if (bitrate_kbps > 0) {
          test::PrintResult(
              "bitrate_stats_",
              (using_min_transmit_bitrate_ ? "min_transmit_bitrate"
                                           : "without_min_transmit_bitrate"),
              "bitrate_kbps",
              static_cast<size_t>(bitrate_kbps),
              "kbps",
              false);
          if (using_min_transmit_bitrate_) {
            if (bitrate_kbps > kMinAcceptableTransmitBitrate &&
                bitrate_kbps < kMaxAcceptableTransmitBitrate) {
              ++num_bitrate_observations_in_range_;
            }
          } else {
            // Expect bitrate stats to roughly match the max encode bitrate.
            if (bitrate_kbps > kMaxEncodeBitrateKbps - 5 &&
                bitrate_kbps < kMaxEncodeBitrateKbps + 5) {
              ++num_bitrate_observations_in_range_;
            }
          }
          if (num_bitrate_observations_in_range_ ==
              kNumBitrateObservationsInRange)
            observation_complete_->Set();
        }
      }
      return send_transport_receiver_->DeliverPacket(packet, length);
    }

    VideoSendStream* send_stream_;
    PacketReceiver* send_transport_receiver_;
    const bool using_min_transmit_bitrate_;
    int num_bitrate_observations_in_range_;
  } observer(pad_to_min_bitrate);

  scoped_ptr<Call> sender_call(
      Call::Create(Call::Config(observer.SendTransport())));
  scoped_ptr<Call> receiver_call(
      Call::Create(Call::Config(observer.ReceiveTransport())));

  VideoSendStream::Config send_config = GetSendTestConfig(sender_call.get());
  fake_encoder_.SetMaxBitrate(kMaxEncodeBitrateKbps);

  observer.SetReceivers(receiver_call->Receiver(), sender_call->Receiver());

  send_config.pacing = true;
  if (pad_to_min_bitrate) {
    send_config.rtp.min_transmit_bitrate_bps = kMinTransmitBitrateBps;
  } else {
    assert(send_config.rtp.min_transmit_bitrate_bps == 0);
  }

  VideoReceiveStream::Config receive_config =
      receiver_call->GetDefaultReceiveConfig();
  receive_config.codecs.clear();
  VideoCodec codec =
      test::CreateDecoderVideoCodec(send_config.encoder_settings);
  receive_config.codecs.push_back(codec);
  test::FakeDecoder fake_decoder;
  ExternalVideoDecoder decoder;
  decoder.decoder = &fake_decoder;
  decoder.payload_type = send_config.encoder_settings.payload_type;
  receive_config.external_decoders.push_back(decoder);
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;

  VideoSendStream* send_stream =
      sender_call->CreateVideoSendStream(send_config);
  VideoReceiveStream* receive_stream =
      receiver_call->CreateVideoReceiveStream(receive_config);
  scoped_ptr<test::FrameGeneratorCapturer> capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream->Input(),
          send_config.encoder_settings.streams[0].width,
          send_config.encoder_settings.streams[0].height,
          30,
          Clock::GetRealTimeClock()));
  observer.SetSendStream(send_stream);
  receive_stream->Start();
  send_stream->Start();
  capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait())
      << "Timeout while waiting for send-bitrate stats.";

  send_stream->Stop();
  receive_stream->Stop();
  observer.StopSending();
  capturer->Stop();
  sender_call->DestroyVideoSendStream(send_stream);
  receiver_call->DestroyVideoReceiveStream(receive_stream);
}

TEST_F(CallPerfTest, PadsToMinTransmitBitrate) { TestMinTransmitBitrate(true); }

TEST_F(CallPerfTest, NoPadWithoutMinTransmitBitrate) {
  TestMinTransmitBitrate(false);
}

}  // namespace webrtc
