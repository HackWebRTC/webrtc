/* Copyright (c) 2026 The WebRTC project authors. */
#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "audio/channel_send.h"
#include "call/rtp_transport_controller_send.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc::roq {
namespace {
class ExternalAudioTransport final : public Transport {
 public:
  RtpPacingMode GetRtpPacingMode() const override {
    return RtpPacingMode::kExternalTransport;
  }
  bool SendRtp(std::span<const uint8_t>, const PacketOptions&) override {
    ++packets;
    return true;
  }
  bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
    return true;
  }
  size_t packets = 0;
};
TEST(ExternalAudioTransportTest, DoesNotWaitForWebRtcNetworkPacer) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1));
  const auto env = CreateTestEnvironment({.time = &time});
  RtpTransportControllerSend controller(
      {.env = env,
       .bitrate_config = {.min_bitrate_bps = 10000,
                          .start_bitrate_bps = 128000,
                          .max_bitrate_bps = 256000},
       .worker_thread = TaskQueueBase::Current()});
  ExternalAudioTransport transport;
  auto channel =
      voe::CreateChannelSend(env, &transport, nullptr, nullptr, CryptoOptions{},
                             false, 1000, 7001, nullptr, &controller);
  auto factory = CreateBuiltinAudioEncoderFactory();
  SdpAudioFormat format("opus", 48000, 2);
  channel->SetEncoder(111, format,
                      factory->Create(env, format, {.payload_type = 111}));
  controller.EnsureStarted();
  channel->RegisterSenderCongestionControlObjects(&controller);
  channel->StartSend();
  controller.OnNetworkAvailability(false);
  for (int i = 0; i < 4; ++i) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = 48000;
    frame->samples_per_channel_ = 480;
    frame->num_channels_ = 1;
    frame->set_absolute_capture_timestamp_ms(
        time.GetClock()->TimeInMilliseconds());
    std::fill_n(frame->mutable_data(), 480, 1);
    channel->ProcessAndEncodeAudio(std::move(frame));
    time.AdvanceTime(TimeDelta::Millis(10));
  }
  EXPECT_GT(transport.packets, 0u);
  channel->StopSend();
  channel->ResetSenderCongestionControlObjects();
}
}  // namespace
}  // namespace webrtc::roq
