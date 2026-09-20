/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_media_engine.h"

#include <arpa/inet.h>
#include <mach-o/dyld.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "api/audio/create_audio_device_module.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/environment/environment_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/fake_video_codec_factory.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_tools/roq/test_roq_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc::roq {
namespace {

class NullTransport final : public Transport {
 public:
  bool SendRtp(std::span<const uint8_t>, const PacketOptions&) override {
    return true;
  }
  bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
    ++rtcp_packets;
    return true;
  }

  std::atomic<size_t> rtcp_packets = 0;
};

class H265RecordingEncoderFactory final : public FakeVideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return {SdpVideoFormat::H265()};
  }
  std::unique_ptr<VideoEncoder> Create(const Environment& env,
                                       const SdpVideoFormat& format) override {
    created_format = format;
    return FakeVideoEncoderFactory::Create(env, format);
  }
  std::optional<SdpVideoFormat> created_format;
};

class H265RecordingDecoderFactory final : public FakeVideoDecoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return {SdpVideoFormat::H265()};
  }
  std::unique_ptr<VideoDecoder> Create(const Environment& env,
                                       const SdpVideoFormat& format) override {
    created_format = format;
    return FakeVideoDecoderFactory::Create(env, format);
  }
  std::optional<SdpVideoFormat> created_format;
};

class H264RecordingEncoderFactory final : public VideoEncoderFactory {
 public:
  explicit H264RecordingEncoderFactory(std::vector<SdpVideoFormat> formats)
      : formats_(std::move(formats)) {}

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return formats_;
  }

  std::unique_ptr<VideoEncoder> Create(const Environment& env,
                                     const SdpVideoFormat& format) override {
    {
      std::lock_guard lock(mutex_);
      created_format_ = format;
    }
    return builtin_->Create(env, format);
  }

  std::optional<SdpVideoFormat> created_format() const {
    std::lock_guard lock(mutex_);
    return created_format_;
  }

 private:
  const std::vector<SdpVideoFormat> formats_;
  const std::unique_ptr<VideoEncoderFactory> builtin_ =
      CreateBuiltinVideoEncoderFactory();
  mutable std::mutex mutex_;
  std::optional<SdpVideoFormat> created_format_;
};

TEST(RoqMediaEngineTest, SelectsAdvertisedH264EncoderFormatByProfile) {
  // Include another legal encoding of Constrained Baseline to catch matching
  // hex prefixes instead of parsing the profile. Level follows the factory.
  for (const char* profile_level_id : {"42e01f", "42e02a", "4d8028"}) {
    SCOPED_TRACE(profile_level_id);
    const Environment env = CreateTestEnvironment();
    auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
    ASSERT_TRUE(audio);
    ASSERT_EQ(audio->Init(), 0);
    const SdpVideoFormat expected(
        "H264", {{"profile-level-id", profile_level_id},
                 {"level-asymmetry-allowed", "1"},
                 {"packetization-mode", "1"}},
        {ScalabilityMode::kL1T1});
    H264RecordingEncoderFactory video_encoder({
        SdpVideoFormat::VP8(),
        SdpVideoFormat("H264", {{"profile-level-id", "42001f"},
                                {"packetization-mode", "1"}}),
        SdpVideoFormat("H264", {{"profile-level-id", "42e01f"},
                                {"packetization-mode", "0"}}),
        expected,
        SdpVideoFormat("H264", {{"profile-level-id", "42e033"},
                                {"packetization-mode", "1"}})});
    auto video_decoder = CreateBuiltinVideoDecoderFactory();
    NullTransport transport;
    std::string error;
    auto engine = RoqMediaEngine::Create(
        {.audio_device_module = audio,
         .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
         .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
         .video_encoder_factory = &video_encoder,
         .video_decoder_factory = video_decoder.get()},
        env, &error);
    ASSERT_TRUE(engine) << error;
    ASSERT_TRUE(engine->Start(&error)) << error;
    ASSERT_TRUE(engine->AddSendBinding(
        {.binding_id = "send-h264",
         .kind = RoqMediaKind::kVideo,
         .primary_ssrc = 3001,
         .rtx_ssrc = 3002,
         .payload_type = 102,
         .rtx_payload_type = 103,
         .transport = &transport,
         .video_min_bitrate_bps = 256000,
         .video_start_bitrate_bps = 1000000,
         .video_max_bitrate_bps = 2000000},
        &error)) << error;
    auto frame = I420Buffer::Create(64, 48);
    I420Buffer::SetBlack(frame.get());
    engine->SendVideoFrame("send-h264", frame, 0);
    for (int attempt = 0; attempt < 100 && !video_encoder.created_format();
         ++attempt)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    engine->Stop();

    ASSERT_TRUE(video_encoder.created_format());
    EXPECT_EQ(*video_encoder.created_format(), expected);
  }
}

TEST(RoqMediaEngineTest, RejectsH264PublicationWithoutMatchingEncoderFormat) {
  for (const std::vector<SdpVideoFormat>& formats : {
           std::vector<SdpVideoFormat>{},
           {SdpVideoFormat::VP8()},
           {SdpVideoFormat("H264", {{"profile-level-id", "42001f"},
                                   {"packetization-mode", "1"}})},
           {SdpVideoFormat("H264", {{"profile-level-id", "42e01f"},
                                   {"packetization-mode", "0"}})},
           {SdpVideoFormat("H264", {{"profile-level-id", "42e01f"}})},
           {SdpVideoFormat("H264", {{"profile-level-id", "invalid"},
                                   {"packetization-mode", "1"}})}}) {
    const Environment env = CreateTestEnvironment();
    auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
    ASSERT_TRUE(audio);
    ASSERT_EQ(audio->Init(), 0);
    H264RecordingEncoderFactory video_encoder(formats);
    auto video_decoder = CreateBuiltinVideoDecoderFactory();
    NullTransport transport;
    std::string error;
    auto engine = RoqMediaEngine::Create(
        {.audio_device_module = audio,
         .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
         .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
         .video_encoder_factory = &video_encoder,
         .video_decoder_factory = video_decoder.get()},
        env, &error);
    ASSERT_TRUE(engine) << error;
    ASSERT_TRUE(engine->Start(&error)) << error;

    EXPECT_FALSE(engine->AddSendBinding(
        {.binding_id = "send-h264",
         .kind = RoqMediaKind::kVideo,
         .primary_ssrc = 3001,
         .rtx_ssrc = 3002,
         .payload_type = 102,
         .rtx_payload_type = 103,
         .transport = &transport,
         .video_min_bitrate_bps = 256000,
         .video_start_bitrate_bps = 1000000,
         .video_max_bitrate_bps = 2000000},
        &error));
    EXPECT_THAT(error, ::testing::HasSubstr("H264"));
    EXPECT_FALSE(engine->HasBinding("send-h264"));
    EXPECT_EQ(engine->binding_count(), 0u);
    engine->Stop();
  }
}

TEST(RoqMediaEngineTest, RoutesH265BindingsToCanonicalH265Factories) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  H265RecordingEncoderFactory video_encoder;
  H265RecordingDecoderFactory video_decoder;
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = &video_encoder,
       .video_decoder_factory = &video_decoder},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "receive-h265",
                                         .kind = RoqMediaKind::kVideo,
                                         .video_codec = kVideoCodecH265,
                                         .primary_ssrc = 2001,
                                         .rtx_ssrc = 2002,
                                         .payload_type = 116,
                                         .rtx_payload_type = 117,
                                         .transport = &transport},
                                        &error))
      << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "send-h265",
                                      .kind = RoqMediaKind::kVideo,
                                      .video_codec = kVideoCodecH265,
                                      .primary_ssrc = 3001,
                                      .rtx_ssrc = 3002,
                                      .payload_type = 116,
                                      .rtx_payload_type = 117,
                                      .transport = &transport,
                                      .video_min_bitrate_bps = 256000,
                                      .video_start_bitrate_bps = 1000000,
                                      .video_max_bitrate_bps = 2000000},
                                     &error))
      << error;
  auto frame = I420Buffer::Create(64, 48);
  I420Buffer::SetBlack(frame.get());
  engine->SendVideoFrame("send-h265", frame, 0);
  for (int attempt = 0; attempt < 100 && !video_encoder.created_format;
       ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  ASSERT_TRUE(video_encoder.created_format.has_value());
  EXPECT_EQ(*video_encoder.created_format, SdpVideoFormat::H265());
  engine->Stop();
}

TEST(RoqMediaEngineTest, StartsPlayoutForTheFirstAudioSubscription) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<test::MockAudioDeviceModule> audio =
      test::MockAudioDeviceModule::CreateNice();
  bool playout_device_selected = false;
  bool playing = false;
  ON_CALL(*audio, Initialized()).WillByDefault(::testing::Return(true));
  ON_CALL(*audio, SetPlayoutDevice(::testing::An<uint16_t>()))
      .WillByDefault([&](uint16_t index) {
        playout_device_selected = index == 0;
        return 0;
      });
  ON_CALL(*audio, InitPlayout()).WillByDefault([&] {
    return playout_device_selected ? 0 : -1;
  });
  ON_CALL(*audio, Playing()).WillByDefault([&] { return playing; });
  ON_CALL(*audio, StartPlayout()).WillByDefault([&] {
    playing = true;
    return 0;
  });
  ON_CALL(*audio, StopPlayout()).WillByDefault([&] {
    playing = false;
    return 0;
  });
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .use_audio_device_playout = true},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  // Device work must be finished before a later subscription can delay PLI.
  EXPECT_TRUE(playing) << "playout must be prepared before installing streams";

  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "speaker",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error))
      << error;

  EXPECT_TRUE(playout_device_selected);
  EXPECT_TRUE(playing);
  ASSERT_TRUE(engine->RemoveBinding("speaker"));
  EXPECT_TRUE(playing)
      << "unsubscribing must not restart the device on the next binding";
  engine->Stop();
  EXPECT_FALSE(playing);
}

TEST(RoqMediaEngineTest, RecoveryFeedbackSurvivesMediaWorkerBacklog) {
  const Environment env = CreateTestEnvironment();
  auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_EQ(audio->Init(), 0);
  auto encoder = CreateBuiltinVideoEncoderFactory();
  auto decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  auto engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = encoder.get(),
       .video_decoder_factory = decoder.get()},
      env, &error);
  ASSERT_TRUE(engine->Start(&error));
  std::atomic<bool> entered = false, release = false;
  engine->EnqueueReceivedPacket({.packet = {1}}, [&](RoqReceivedPacket) {
    entered = true;
    while (!release.load())
      std::this_thread::yield();
  });
  for (int i = 0; i < 1000 && !entered; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(entered);
  for (int i = 0; i < 512; ++i)
    engine->EnqueueReceivedPacket({.packet = std::vector<uint8_t>(1000)},
                                  [](RoqReceivedPacket) {});
  std::atomic<int> feedback = 0;
  // A valid PLI that has already waited longer than the media ingress deadline.
  auto received_at = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count() -
                     300000;
  engine->EnqueueReceivedPacket(
      {.packet = {0x81, 206, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2},
       .received_at_us = received_at},
      [&](RoqReceivedPacket) { ++feedback; });
  release = true;
  for (int i = 0; i < 1000 && feedback == 0; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  engine->Stop();
  EXPECT_EQ(feedback.load(), 1);
}

TEST(RoqMediaEngineTest, BoundedIngressReturnsWhileWorkerIsBlocked) {
  const Environment env = CreateTestEnvironment();
  auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_EQ(audio->Init(), 0);
  auto encoder = CreateBuiltinVideoEncoderFactory();
  auto decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  auto engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = encoder.get(),
       .video_decoder_factory = decoder.get()},
      env, &error);
  ASSERT_TRUE(engine->Start(&error));
  std::atomic<bool> entered = false, release = false;
  engine->EnqueueReceivedPacket({.packet = {1}}, [&](RoqReceivedPacket) {
    entered = true;
    while (!release.load())
      std::this_thread::yield();
  });
  for (int i = 0; i < 1000 && !entered; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(entered);
  std::atomic<int> delivered = 0;
  for (int i = 0; i < 600; ++i)
    engine->EnqueueReceivedPacket({.packet = std::vector<uint8_t>(1400)},
                                  [&](RoqReceivedPacket) { ++delivered; });
  release = true;
  engine->stats();
  engine->Stop();
  EXPECT_LE(delivered.load(),
            374);  // floor(512 KiB / 1400), independent bound.
}

TEST(RoqMediaEngineTest, QuicProgressesWhileMediaWorkerIsBlocked) {
  const Environment env = CreateTestEnvironment();
  auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_EQ(audio->Init(), 0);
  auto encoder = CreateBuiltinVideoEncoderFactory();
  auto decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  auto engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = encoder.get(),
       .video_decoder_factory = decoder.get()},
      env, &error);
  ASSERT_TRUE(engine->Start(&error));
  const int fd = socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_GE(fd, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ASSERT_EQ(bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)),
            0);
  socklen_t size = sizeof(address);
  ASSERT_EQ(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size), 0);
  const auto endpoint = "127.0.0.1:" + std::to_string(ntohs(address.sin_port));
  close(fd);
  uint32_t path_size = 0;
  _NSGetExecutablePath(nullptr, &path_size);
  std::string executable(path_size, '\0');
  ASSERT_EQ(_NSGetExecutablePath(executable.data(), &path_size), 0);
  const auto binary_dir =
      std::filesystem::path(executable.c_str()).parent_path();
  std::atomic<int> server_received = 0, client_control = 0, callbacks = 0,
                   delivered = 0;
  auto server = CreateTestRoqServer(
      {.address = endpoint,
       .certificate_path = (binary_dir / "cert.crt").string(),
       .private_key_path = (binary_dir / "cert.key").string(),
       .allowed_flow_ids = {8}},
      [&](RoqReceivedPacket) { ++server_received; });
  ASSERT_TRUE(server->Start(&error));
  ASSERT_TRUE(server->WaitUntilReady(TimeDelta::Seconds(2)));
  auto client = RoqSession::Create(
      {.address = endpoint,
       .trusted_ca_file = (binary_dir / "cert.crt").string(),
       .allowed_flow_ids = {8}},
      [&](RoqReceivedPacket packet) {
        engine->EnqueueReceivedPacket(std::move(packet),
                                      [&](RoqReceivedPacket) { ++delivered; });
        ++callbacks;
      },
      {}, [&](std::vector<uint8_t>) { ++client_control; });
  // Stream 0 is client initiated; open it before the server replies.
  ASSERT_TRUE(client->SendReplaySafeMessage(std::vector<uint8_t>{1}));
  ASSERT_TRUE(client->Start(&error));
  ASSERT_TRUE(client->WaitUntilEstablished(TimeDelta::Seconds(2)))
      << client->error();
  for (int i = 0; i < 1000 && server->stats().reliable_messages_received == 0;
       ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT_GT(server->stats().reliable_messages_received, 0u);
  std::atomic<bool> entered = false, release = false;
  engine->EnqueueReceivedPacket({.packet = {1}}, [&](RoqReceivedPacket) {
    entered = true;
    while (!release.load())
      std::this_thread::yield();
  });
  for (int i = 0; i < 1000 && !entered; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(entered);
  const auto acked_before = client->stats().acked_bytes;
  const std::vector<uint8_t> packet = {0x80, 96, 0, 1, 0, 0,   0,
                                       1,    0,  0, 0, 1, 0x65};
  for (int i = 0; i < 8; ++i) {
    server->SendMedia(8, packet, {}, {});
    client->SendMedia(8, packet, {}, {});
  }
  server->SendReplaySafeMessage(packet);
  client->RequestPing();
  for (int i = 0; i < 1000 &&
                  (callbacks < 8 || server_received < 8 || client_control < 1 ||
                   client->stats().acked_bytes <= acked_before);
       ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_EQ(callbacks, 8);
  EXPECT_EQ(server_received, 8);
  EXPECT_EQ(client_control, 1);
  EXPECT_GT(client->stats().acked_bytes, acked_before);
  EXPECT_EQ(delivered, 0);
  release = true;
  EXPECT_TRUE(client->error().empty()) << client->error();
  EXPECT_TRUE(server->error().empty()) << server->error();
  client->Close();
  server->Close();
  client->WaitUntilClosed(TimeDelta::Seconds(2));
  server->WaitUntilClosed(TimeDelta::Seconds(2));
  engine->Stop();
}

TEST(RoqMediaEngineTest, RejectsAudioSubscriptionWithoutAPlayoutDevice) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<test::MockAudioDeviceModule> audio =
      test::MockAudioDeviceModule::CreateNice();
  ON_CALL(*audio, Initialized()).WillByDefault(::testing::Return(true));
  ON_CALL(*audio, SetPlayoutDevice(::testing::An<uint16_t>()))
      .WillByDefault(::testing::Return(-1));
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .use_audio_device_playout = true},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  EXPECT_FALSE(engine->AddReceiveBinding({.binding_id = "speaker",
                                          .kind = RoqMediaKind::kAudio,
                                          .primary_ssrc = 1001,
                                          .payload_type = 63,
                                          .transport = &transport},
                                         &error));
  EXPECT_EQ(error, "failed to start audio playout device");
  EXPECT_EQ(engine->binding_count(), 0u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, StartsRecordingForTheFirstDeviceAudioPublication) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<test::MockAudioDeviceModule> audio =
      test::MockAudioDeviceModule::CreateNice();
  bool recording_device_selected = false;
  bool recording = false;
  ON_CALL(*audio, Initialized()).WillByDefault(::testing::Return(true));
  ON_CALL(*audio, SetRecordingDevice(::testing::An<uint16_t>()))
      .WillByDefault([&](uint16_t index) {
        recording_device_selected = index == 0;
        return 0;
      });
  ON_CALL(*audio, InitRecording()).WillByDefault([&] {
    return recording_device_selected ? 0 : -1;
  });
  ON_CALL(*audio, Recording()).WillByDefault([&] { return recording; });
  ON_CALL(*audio, StartRecording()).WillByDefault([&] {
    recording = true;
    return 0;
  });
  ON_CALL(*audio, StopRecording()).WillByDefault([&] {
    recording = false;
    return 0;
  });
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  ASSERT_TRUE(
      engine->AddSendBinding({.binding_id = "microphone",
                              .kind = RoqMediaKind::kAudio,
                              .primary_ssrc = 1001,
                              .payload_type = 63,
                              .transport = &transport,
                              .audio_input_mode = RoqAudioInputMode::kDevice,
                              .audio_bitrate_bps = 128000},
                             &error))
      << error;

  EXPECT_TRUE(recording_device_selected);
  EXPECT_TRUE(recording);
  engine->Stop();
}

TEST(RoqMediaEngineTest, EnablesAecNoiseSuppressionAndAgcForDeviceAudio) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .use_audio_device_recording = true},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  const RoqMediaEngineStats stats = engine->stats();
  EXPECT_TRUE(stats.audio_processing_active);
  EXPECT_TRUE(stats.echo_cancellation_enabled);
  EXPECT_TRUE(stats.noise_suppression_enabled);
  EXPECT_TRUE(stats.automatic_gain_control_enabled);
  engine->Stop();
}

TEST(RoqMediaEngineTest, ExternalPcmDoesNotEnableAudioProcessing) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .use_audio_device_recording = false},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  const RoqMediaEngineStats stats = engine->stats();
  EXPECT_FALSE(stats.audio_processing_active);
  EXPECT_FALSE(stats.echo_cancellation_enabled);
  EXPECT_FALSE(stats.noise_suppression_enabled);
  EXPECT_FALSE(stats.automatic_gain_control_enabled);
  engine->Stop();
}

TEST(RoqMediaEngineTest, EnablesNackForOpusSendAndReceiveBindings) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &transport,
                                      .audio_bitrate_bps = 128000},
                                     &error))
      << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "speaker",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 2001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error))
      << error;

  const RoqMediaEngineStats stats = engine->stats();
  EXPECT_TRUE(stats.audio_send_nack_enabled);
  EXPECT_TRUE(stats.audio_receive_nack_enabled);
  engine->Stop();
}

TEST(RoqMediaEngineTest, AllowsAudioReceiveNackToBeDisabled) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "speaker",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 2001,
                                         .payload_type = 63,
                                         .transport = &transport,
                                         .enable_nack = false},
                                        &error));

  EXPECT_FALSE(engine->stats().audio_receive_nack_enabled);
  engine->Stop();
}

TEST(RoqMediaEngineTest, ExplicitVideoKeyFrameRequestSendsRtcpImmediately) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "camera",
                                         .kind = RoqMediaKind::kVideo,
                                         .primary_ssrc = 2001,
                                         .rtx_ssrc = 2002,
                                         .payload_type = 96,
                                         .rtx_payload_type = 97,
                                         .transport = &transport},
                                        &error))
      << error;

  engine->RequestReceiverKeyFrame("camera");

  for (int attempt = 0; attempt < 100 && transport.rtcp_packets.load() == 0;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_GT(transport.rtcp_packets.load(), 0u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, CountsAudioAndVideoSenderNackStats) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &transport,
                                      .audio_bitrate_bps = 128000},
                                     &error));
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "camera",
                                      .kind = RoqMediaKind::kVideo,
                                      .primary_ssrc = 2001,
                                      .rtx_ssrc = 2002,
                                      .payload_type = 96,
                                      .rtx_payload_type = 97,
                                      .transport = &transport,
                                      .video_min_bitrate_bps = 256000,
                                      .video_start_bitrate_bps = 1000000,
                                      .video_max_bitrate_bps = 2000000},
                                     &error));

  rtcp::Nack audio_nack;
  audio_nack.SetSenderSsrc(3001);
  audio_nack.SetMediaSsrc(1001);
  audio_nack.SetPacketIds(std::vector<uint16_t>{1});
  const Buffer audio_packet = audio_nack.Build();
  engine->DeliverRtcp("microphone", audio_packet);
  rtcp::Nack video_nack;
  video_nack.SetSenderSsrc(3002);
  video_nack.SetMediaSsrc(2001);
  video_nack.SetPacketIds(std::vector<uint16_t>{1});
  const Buffer video_packet = video_nack.Build();
  engine->DeliverRtcp("camera", video_packet);
  engine->DeliverRtcp("camera", video_packet);

  RoqMediaEngineStats stats;
  for (int attempt = 0; attempt < 100; ++attempt) {
    stats = engine->stats();
    if (stats.audio_nack_received == 1 && stats.video_nack_received == 2)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_EQ(stats.nack_received, 3u);
  EXPECT_EQ(stats.audio_nack_received, 1u);
  EXPECT_EQ(stats.video_nack_received, 2u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, StopsRecordingAfterTheLastDevicePublicationCloses) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<test::MockAudioDeviceModule> audio =
      test::MockAudioDeviceModule::CreateNice();
  bool recording = false;
  ON_CALL(*audio, Initialized()).WillByDefault(::testing::Return(true));
  ON_CALL(*audio, SetRecordingDevice(::testing::An<uint16_t>()))
      .WillByDefault(::testing::Return(0));
  ON_CALL(*audio, InitRecording()).WillByDefault(::testing::Return(0));
  ON_CALL(*audio, Recording()).WillByDefault([&] { return recording; });
  ON_CALL(*audio, StartRecording()).WillByDefault([&] {
    recording = true;
    return 0;
  });
  ON_CALL(*audio, StopRecording()).WillByDefault([&] {
    recording = false;
    return 0;
  });
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine);
  ASSERT_TRUE(engine->Start(&error));
  ASSERT_TRUE(
      engine->AddSendBinding({.binding_id = "microphone",
                              .kind = RoqMediaKind::kAudio,
                              .primary_ssrc = 1001,
                              .payload_type = 63,
                              .transport = &transport,
                              .audio_input_mode = RoqAudioInputMode::kDevice,
                              .audio_bitrate_bps = 128000},
                             &error));
  ASSERT_TRUE(recording);

  EXPECT_TRUE(engine->RemoveBinding("microphone"));
  EXPECT_FALSE(recording);
  engine->Stop();
}

TEST(RoqMediaEngineTest, PacketizesLargeVideoFramesForTheRoqMtu) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  class MeasuringTransport final : public Transport {
   public:
    bool SendRtp(std::span<const uint8_t> packet,
                 const PacketOptions&) override {
      ++packets;
      maximum_packet_size = std::max(maximum_packet_size.load(), packet.size());
      return true;
    }
    bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
      return true;
    }
    std::atomic<size_t> packets = 0;
    std::atomic<size_t> maximum_packet_size = 0;
  } transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .max_rtp_packet_size = 1200},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "video",
                                      .kind = RoqMediaKind::kVideo,
                                      .primary_ssrc = 1001,
                                      .rtx_ssrc = 1002,
                                      .payload_type = 96,
                                      .rtx_payload_type = 97,
                                      .transport = &transport,
                                      .video_min_bitrate_bps = 256000,
                                      .video_start_bitrate_bps = 1000000,
                                      .video_max_bitrate_bps = 2000000},
                                     &error))
      << error;
  auto frame = I420Buffer::Create(1920, 1080);
  I420Buffer::SetBlack(frame.get());
  for (int index = 0; index < 90; ++index) {
    engine->SendVideoFrame("video", frame, index * 16667);
    std::this_thread::sleep_for(std::chrono::milliseconds(17));
  }

  for (int attempt = 0; attempt < 500 && transport.packets.load() == 0;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_GT(transport.packets.load(), 0u);
  EXPECT_LE(transport.maximum_packet_size.load(), 1200u);
  const RoqMediaEngineStats stats = engine->stats();
  EXPECT_GT(stats.video_frames_encoded, 0u);
  EXPECT_GT(stats.video_target_bitrate_bps, 0);
  EXPECT_GT(stats.video_media_bitrate_bps, 0);
  engine->Stop();
}

TEST(RoqMediaEngineTest, ShrinksVideoPacketizationAfterPmtuUpdate) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  class MeasuringTransport final : public Transport {
   public:
    bool SendRtp(std::span<const uint8_t> packet,
                 const PacketOptions&) override {
      if (measure.load()) {
        ++packets;
        maximum_packet_size =
            std::max(maximum_packet_size.load(), packet.size());
      }
      return true;
    }
    bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
      return true;
    }
    std::atomic<bool> measure = false;
    std::atomic<size_t> packets = 0;
    std::atomic<size_t> maximum_packet_size = 0;
  } transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .max_rtp_packet_size = 1200},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "video",
                                      .kind = RoqMediaKind::kVideo,
                                      .primary_ssrc = 1001,
                                      .rtx_ssrc = 1002,
                                      .payload_type = 96,
                                      .rtx_payload_type = 97,
                                      .transport = &transport,
                                      .video_min_bitrate_bps = 256000,
                                      .video_start_bitrate_bps = 1000000,
                                      .video_max_bitrate_bps = 2000000},
                                     &error))
      << error;

  engine->UpdateMaxRtpPacketSize(700);
  transport.measure.store(true);
  auto frame = I420Buffer::Create(1920, 1080);
  I420Buffer::SetBlack(frame.get());
  for (int index = 0; index < 90; ++index) {
    engine->SendVideoFrame("video", frame, index * 16667);
    std::this_thread::sleep_for(std::chrono::milliseconds(17));
  }
  for (int attempt = 0; attempt < 500 && transport.packets.load() == 0;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_GT(transport.packets.load(), 0u);
  EXPECT_LE(transport.maximum_packet_size.load(), 700u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, ManagesMultipleBindingsInOneCall) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport first_transport;
  NullTransport second_transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  EXPECT_TRUE(engine->AddReceiveBinding({.binding_id = "first",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &first_transport},
                                        &error))
      << error;
  EXPECT_TRUE(engine->AddReceiveBinding({.binding_id = "second",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1002,
                                         .payload_type = 63,
                                         .transport = &second_transport},
                                        &error))
      << error;
  EXPECT_EQ(engine->binding_count(), 2u);

  EXPECT_TRUE(engine->RemoveBinding("first"));
  EXPECT_EQ(engine->binding_count(), 1u);
  EXPECT_TRUE(engine->HasBinding("second"));
  EXPECT_FALSE(engine->HasBinding("first"));

  engine->Stop();
}

TEST(RoqMediaEngineTest, RejectsDuplicateBindingAndSsrc) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  const RoqMediaBindingConfig first{.binding_id = "first",
                                    .kind = RoqMediaKind::kAudio,
                                    .primary_ssrc = 1001,
                                    .payload_type = 63,
                                    .transport = &transport};
  ASSERT_TRUE(engine->AddReceiveBinding(first, &error)) << error;

  EXPECT_FALSE(engine->AddReceiveBinding(first, &error));
  EXPECT_EQ(error, "binding ID is already active");
  EXPECT_FALSE(engine->AddReceiveBinding({.binding_id = "second",
                                          .kind = RoqMediaKind::kAudio,
                                          .primary_ssrc = 1001,
                                          .payload_type = 63,
                                          .transport = &transport},
                                         &error));
  EXPECT_EQ(error, "SSRC is already active");

  engine->Stop();
}

TEST(RoqMediaEngineTest, AddsIndependentSendBindingsToTheSameCall) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport audio_transport;
  NullTransport video_transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;

  EXPECT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &audio_transport,
                                      .audio_bitrate_bps = 128000},
                                     &error))
      << error;
  EXPECT_FALSE(audio->Recording());
  EXPECT_TRUE(engine->AddSendBinding({.binding_id = "camera",
                                      .kind = RoqMediaKind::kVideo,
                                      .primary_ssrc = 1002,
                                      .rtx_ssrc = 1003,
                                      .payload_type = 96,
                                      .rtx_payload_type = 97,
                                      .transport = &video_transport,
                                      .video_min_bitrate_bps = 256000,
                                      .video_start_bitrate_bps = 1000000,
                                      .video_max_bitrate_bps = 2000000},
                                     &error))
      << error;
  EXPECT_EQ(engine->binding_count(), 2u);

  EXPECT_TRUE(engine->RemoveBinding("microphone"));
  EXPECT_TRUE(engine->HasBinding("camera"));
  EXPECT_EQ(engine->binding_count(), 1u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, QuicControllerDoesNotProbePadOrUseCongestionWindow) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  const RoqSessionStats network = {
      .rtt_ns = 50'000'000,
      .delivery_rate_bytes_per_second = 250'000,
      .max_bandwidth_bytes_per_second = 500'000,
      .acked_bytes = 1000,
      .startup_exit_cwnd = 12000,
  };
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .network_stats_provider = [&] { return network; },
       .send_min_bitrate_bps = 256000,
       .send_start_bitrate_bps = 1000000,
       .send_max_bitrate_bps = 2000000},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  NullTransport transport;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &transport,
                                      .audio_bitrate_bps = 128000},
                                     &error))
      << error;

  RoqMediaEngineStats stats;
  for (int attempt = 0; attempt < 100; ++attempt) {
    stats = engine->stats();
    if (stats.network_control_updates > 0)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_GT(stats.network_control_updates, 0u);
  EXPECT_GT(stats.controller_target_rate_bps, 0);
  EXPECT_EQ(stats.controller_pacer_rate_bps, stats.controller_target_rate_bps);
  EXPECT_EQ(stats.controller_padding_rate_bps, 0);
  EXPECT_EQ(stats.controller_probe_cluster_count, 0u);
  EXPECT_FALSE(stats.controller_has_congestion_window);
  engine->Stop();
}

TEST(RoqMediaEngineTest, DynamicSendBitrateConstraintsReachQuicController) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get(),
       .network_stats_provider = [] { return RoqSessionStats{}; },
       .send_min_bitrate_bps = 128000,
       .send_start_bitrate_bps = 128000,
       .send_max_bitrate_bps = 128000},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  NullTransport transport;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &transport,
                                      .audio_bitrate_bps = 128000},
                                     &error))
      << error;

  engine->UpdateSendBitrateConstraints(384000, 1128000, 2128000);

  RoqMediaEngineStats stats;
  for (int attempt = 0; attempt < 100; ++attempt) {
    stats = engine->stats();
    if (stats.controller_target_rate_bps == 1128000)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_EQ(stats.controller_target_rate_bps, 1128000);
  EXPECT_EQ(stats.controller_pacer_rate_bps, 1128000);
  EXPECT_EQ(stats.call_target_rate_bps, 1128000);
  engine->Stop();
}

TEST(RoqMediaEngineTest, CountsAcceptedExternalAudioPackets) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  class RecordingTransport final : public Transport {
   public:
    bool SendRtp(std::span<const uint8_t> packet,
                 const PacketOptions&) override {
      std::lock_guard lock(mutex);
      if (first_packet.empty())
        first_packet.assign(packet.begin(), packet.end());
      return true;
    }
    bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
      return true;
    }
    std::mutex mutex;
    std::vector<uint8_t> first_packet;
  } transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddSendBinding({.binding_id = "microphone",
                                      .kind = RoqMediaKind::kAudio,
                                      .primary_ssrc = 1001,
                                      .payload_type = 63,
                                      .transport = &transport,
                                      .audio_bitrate_bps = 128000},
                                     &error));

  std::vector<int16_t> samples(960);
  for (uint32_t index = 0; index < 120; ++index)
    engine->SendAudioBlock("microphone", samples, index * 480);

  bool sent = false;
  for (int attempt = 0; attempt < 300 && !sent; ++attempt) {
    sent = engine->stats().sent_audio_packets > 0;
    if (!sent)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_TRUE(sent);
  ASSERT_TRUE(engine->RemoveBinding("microphone"));
  EXPECT_GT(engine->stats().sent_audio_packets, 0);
  engine->Stop();
  RtpPacketReceived packet;
  std::lock_guard lock(transport.mutex);
  ASSERT_TRUE(packet.Parse(transport.first_packet));
  EXPECT_EQ(packet.PayloadType(), 63);
  ASSERT_FALSE(packet.payload().empty());
  EXPECT_EQ(packet.payload()[0], 111)
      << "RED primary block must identify LiveKit's Opus payload type";
}

TEST(RoqMediaEngineTest, DecodesCanonicalRedOpusBeforeAndAfterResume) {
  const Environment env = CreateTestEnvironment();
  auto audio = CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  auto engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()}, env, &error);
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding(
      {.binding_id = "audio", .kind = RoqMediaKind::kAudio,
       .primary_ssrc = 1001, .payload_type = 63, .transport = &transport},
      &error)) << error;

  for (int generation = 0; generation < 2; ++generation) {
    if (generation != 0) {
      ASSERT_TRUE(engine->SuspendReceiveBinding("audio"));
      ASSERT_TRUE(engine->ResumeReceiveBinding("audio", &error)) << error;
    }
    // RTP PT 63, one RED primary block for PT 111, and a valid Opus
    // silence frame. This fixture is independent of the RoQ encoder.
    const std::array<uint8_t, 16> packet = {
        0x80, 63, 0, 1, 0, 0, 3, 0xc0, 0, 0, 3, 0xe9,
        111, 0xfc, 0xff, 0xfe};
    engine->DeliverRtp("audio", packet);
    int decoded = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
      decoded = engine->stats().audio_decoding_normal;
      if (decoded > 0)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_GT(decoded, 0) << "generation " << generation;
  }
  engine->Stop();
}

TEST(RoqMediaEngineTest, QueriesStatsByStableBindingId) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "subscription",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error));

  EXPECT_TRUE(engine->binding_stats("subscription").has_value());
  EXPECT_FALSE(engine->binding_stats("missing").has_value());
}

TEST(RoqMediaEngineTest, ReportsReceivedVideoPacketsByBinding) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "camera",
                                         .kind = RoqMediaKind::kVideo,
                                         .primary_ssrc = 2001,
                                         .rtx_ssrc = 2002,
                                         .payload_type = 96,
                                         .rtx_payload_type = 97,
                                         .transport = &transport},
                                        &error))
      << error;
  constexpr std::array<uint8_t, 14> packet = {
      0x80, 0xe0, 0x00, 0x01, 0x00, 0x01, 0x5f,
      0x90, 0x00, 0x00, 0x07, 0xd1, 0x65, 0x80};

  engine->DeliverRtp("camera", packet);
  std::optional<RoqMediaBindingStats> stats;
  for (int attempt = 0; attempt < 100; ++attempt) {
    stats = engine->binding_stats("camera");
    if (stats && stats->received_video_packets == 1)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  ASSERT_TRUE(stats);
  EXPECT_EQ(stats->received_audio_packets, 0u);
  EXPECT_EQ(stats->received_video_packets, 1u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, RetainsTerminalBindingStatsAfterRemoval) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "subscription",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error));

  ASSERT_TRUE(engine->RemoveBinding("subscription"));

  EXPECT_FALSE(engine->HasBinding("subscription"));
  EXPECT_EQ(engine->binding_count(), 0u);
  EXPECT_TRUE(engine->binding_stats("subscription").has_value());
  engine->Stop();
}

TEST(RoqMediaEngineTest, SuspendsReceiveBindingWithoutRemovingIt) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "subscription",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error));

  EXPECT_TRUE(engine->SuspendReceiveBinding("subscription"));
  EXPECT_TRUE(engine->HasBinding("subscription"));
  EXPECT_EQ(engine->binding_count(), 1u);
  engine->Stop();
}

TEST(RoqMediaEngineTest, ResumesSuspendedReceiveBinding) {
  const Environment env = CreateTestEnvironment();
  scoped_refptr<AudioDeviceModule> audio =
      CreateAudioDeviceModule(env, AudioDeviceModule::kDummyAudio);
  ASSERT_TRUE(audio);
  ASSERT_EQ(audio->Init(), 0);
  auto video_encoder = CreateBuiltinVideoEncoderFactory();
  auto video_decoder = CreateBuiltinVideoDecoderFactory();
  NullTransport transport;
  std::string error;
  std::unique_ptr<RoqMediaEngine> engine = RoqMediaEngine::Create(
      {.audio_device_module = audio,
       .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
       .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
       .video_encoder_factory = video_encoder.get(),
       .video_decoder_factory = video_decoder.get()},
      env, &error);
  ASSERT_TRUE(engine) << error;
  ASSERT_TRUE(engine->Start(&error)) << error;
  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "subscription",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 63,
                                         .transport = &transport},
                                        &error));
  ASSERT_TRUE(engine->SuspendReceiveBinding("subscription"));

  EXPECT_TRUE(engine->ResumeReceiveBinding("subscription", &error)) << error;
  EXPECT_TRUE(engine->HasBinding("subscription"));
  engine->Stop();
}

}  // namespace
}  // namespace webrtc::roq
