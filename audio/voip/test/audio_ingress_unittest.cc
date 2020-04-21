/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_ingress.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "audio/voip/audio_egress.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Unused;

constexpr int16_t kAudioLevel = 3004;  // Used for sine wave level.

std::unique_ptr<RtpRtcp> CreateRtpStack(Clock* clock, Transport* transport) {
  RtpRtcp::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.rtcp_report_interval_ms = 5000;
  rtp_config.outgoing_transport = transport;
  rtp_config.local_media_ssrc = 0xdeadc0de;
  auto rtp_rtcp = RtpRtcp::Create(rtp_config);
  rtp_rtcp->SetSendingMediaStatus(false);
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
  return rtp_rtcp;
}

class AudioIngressTest : public ::testing::Test {
 public:
  const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

  AudioIngressTest()
      : fake_clock_(123456789), wave_generator_(1000.0, kAudioLevel) {
    rtp_rtcp_ = CreateRtpStack(&fake_clock_, &transport_);
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
    decoder_factory_ = CreateBuiltinAudioDecoderFactory();
  }

  void SetUp() override {
    constexpr int kPcmuPayload = 0;
    ingress_ = std::make_unique<AudioIngress>(
        rtp_rtcp_.get(), &fake_clock_, decoder_factory_,
        ReceiveStatistics::Create(&fake_clock_));
    ingress_->SetReceiveCodecs({{kPcmuPayload, kPcmuFormat}});

    egress_ = std::make_unique<AudioEgress>(rtp_rtcp_.get(), &fake_clock_,
                                            task_queue_factory_.get());
    egress_->SetEncoder(kPcmuPayload, kPcmuFormat,
                        encoder_factory_->MakeAudioEncoder(
                            kPcmuPayload, kPcmuFormat, absl::nullopt));
    egress_->StartSend();
    ingress_->StartPlay();
    rtp_rtcp_->SetSendingStatus(true);
  }

  void TearDown() override {
    rtp_rtcp_->SetSendingStatus(false);
    ingress_->StopPlay();
    egress_->StopSend();
  }

  std::unique_ptr<AudioFrame> GetAudioFrame(int order) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kPcmuFormat.clockrate_hz;
    frame->samples_per_channel_ = kPcmuFormat.clockrate_hz / 100;  // 10 ms.
    frame->num_channels_ = kPcmuFormat.num_channels;
    frame->timestamp_ = frame->samples_per_channel_ * order;
    wave_generator_.GenerateNextFrame(frame.get());
    return frame;
  }

  SimulatedClock fake_clock_;
  SineWaveGenerator wave_generator_;
  NiceMock<MockTransport> transport_;
  std::unique_ptr<AudioIngress> ingress_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  // Members used to drive the input to ingress.
  std::unique_ptr<AudioEgress> egress_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::shared_ptr<RtpRtcp> rtp_rtcp_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
};

TEST_F(AudioIngressTest, PlayingAfterStartAndStop) {
  EXPECT_EQ(ingress_->Playing(), true);
  ingress_->StopPlay();
  EXPECT_EQ(ingress_->Playing(), false);
}

TEST_F(AudioIngressTest, GetAudioFrameAfterRtpReceived) {
  rtc::Event event;
  auto handle_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    ingress_->ReceivedRTPPacket(packet, length);
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(handle_rtp));
  egress_->SendAudioData(GetAudioFrame(0));
  egress_->SendAudioData(GetAudioFrame(1));
  event.Wait(/*ms=*/1000);

  AudioFrame audio_frame;
  EXPECT_EQ(
      ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
      AudioMixer::Source::AudioFrameInfo::kNormal);
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_EQ(audio_frame.num_channels_, 1u);
  EXPECT_EQ(audio_frame.samples_per_channel_,
            static_cast<size_t>(kPcmuFormat.clockrate_hz / 100));
  EXPECT_EQ(audio_frame.sample_rate_hz_, kPcmuFormat.clockrate_hz);
  EXPECT_NE(audio_frame.timestamp_, 0u);
  EXPECT_EQ(audio_frame.elapsed_time_ms_, 0);
}

TEST_F(AudioIngressTest, GetSpeechOutputLevelFullRange) {
  // Per audio_level's kUpdateFrequency, we need 11 RTP to get audio level.
  constexpr int kNumRtp = 11;
  int rtp_count = 0;
  rtc::Event event;
  auto handle_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    ingress_->ReceivedRTPPacket(packet, length);
    if (++rtp_count == kNumRtp) {
      event.Set();
    }
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(handle_rtp));
  for (int i = 0; i < kNumRtp * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    fake_clock_.AdvanceTimeMilliseconds(10);
  }
  event.Wait(/*ms=*/1000);

  for (int i = 0; i < kNumRtp; ++i) {
    AudioFrame audio_frame;
    EXPECT_EQ(
        ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
        AudioMixer::Source::AudioFrameInfo::kNormal);
  }
  EXPECT_EQ(ingress_->GetSpeechOutputLevelFullRange(), kAudioLevel);
}

TEST_F(AudioIngressTest, PreferredSampleRate) {
  rtc::Event event;
  auto handle_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    ingress_->ReceivedRTPPacket(packet, length);
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(handle_rtp));
  egress_->SendAudioData(GetAudioFrame(0));
  egress_->SendAudioData(GetAudioFrame(1));
  event.Wait(/*ms=*/1000);

  AudioFrame audio_frame;
  EXPECT_EQ(
      ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
      AudioMixer::Source::AudioFrameInfo::kNormal);
  EXPECT_EQ(ingress_->PreferredSampleRate(), kPcmuFormat.clockrate_hz);
}

}  // namespace
}  // namespace webrtc
