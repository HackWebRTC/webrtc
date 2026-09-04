/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_media_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "api/audio/create_audio_device_module.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/environment/environment_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/audio_device/include/mock_audio_device.h"
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
    return true;
  }
};

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
  ON_CALL(*audio, Playing()).WillByDefault([&] {
    return playing;
  });
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

  ASSERT_TRUE(engine->AddReceiveBinding({.binding_id = "speaker",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1001,
                                         .payload_type = 111,
                                         .transport = &transport},
                                        &error))
      << error;

  EXPECT_TRUE(playout_device_selected);
  EXPECT_TRUE(playing);
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
                                          .payload_type = 111,
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
  ON_CALL(*audio, Recording()).WillByDefault([&] {
    return recording;
  });
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

  ASSERT_TRUE(engine->AddSendBinding(
      {.binding_id = "microphone",
       .kind = RoqMediaKind::kAudio,
       .primary_ssrc = 1001,
       .payload_type = 111,
       .transport = &transport,
       .audio_input_mode = RoqAudioInputMode::kDevice},
      &error))
      << error;

  EXPECT_TRUE(recording_device_selected);
  EXPECT_TRUE(recording);
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
  ON_CALL(*audio, StartRecording()).WillByDefault([&] { recording = true; return 0; });
  ON_CALL(*audio, StopRecording()).WillByDefault([&] { recording = false; return 0; });
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
  ASSERT_TRUE(engine->AddSendBinding(
      {.binding_id = "microphone", .kind = RoqMediaKind::kAudio,
       .primary_ssrc = 1001, .payload_type = 111, .transport = &transport,
       .audio_input_mode = RoqAudioInputMode::kDevice}, &error));
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
      maximum_packet_size =
          std::max(maximum_packet_size.load(), packet.size());
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
                                      .transport = &transport},
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
                                         .payload_type = 111,
                                         .transport = &first_transport},
                                        &error))
      << error;
  EXPECT_TRUE(engine->AddReceiveBinding({.binding_id = "second",
                                         .kind = RoqMediaKind::kAudio,
                                         .primary_ssrc = 1002,
                                         .payload_type = 111,
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
                                    .payload_type = 111,
                                    .transport = &transport};
  ASSERT_TRUE(engine->AddReceiveBinding(first, &error)) << error;

  EXPECT_FALSE(engine->AddReceiveBinding(first, &error));
  EXPECT_EQ(error, "binding ID is already active");
  EXPECT_FALSE(engine->AddReceiveBinding({.binding_id = "second",
                                          .kind = RoqMediaKind::kAudio,
                                          .primary_ssrc = 1001,
                                          .payload_type = 111,
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
                                      .payload_type = 111,
                                      .transport = &audio_transport},
                                     &error))
      << error;
  EXPECT_FALSE(audio->Recording());
  EXPECT_TRUE(engine->AddSendBinding({.binding_id = "camera",
                                      .kind = RoqMediaKind::kVideo,
                                      .primary_ssrc = 1002,
                                      .rtx_ssrc = 1003,
                                      .payload_type = 96,
                                      .rtx_payload_type = 97,
                                      .transport = &video_transport},
                                     &error))
      << error;
  EXPECT_EQ(engine->binding_count(), 2u);

  EXPECT_TRUE(engine->RemoveBinding("microphone"));
  EXPECT_TRUE(engine->HasBinding("camera"));
  EXPECT_EQ(engine->binding_count(), 1u);
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
  ASSERT_TRUE(engine->AddSendBinding(
      {.binding_id = "microphone", .kind = RoqMediaKind::kAudio,
       .primary_ssrc = 1001, .payload_type = 111, .transport = &transport},
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
                                         .payload_type = 111,
                                         .transport = &transport},
                                        &error));

  EXPECT_TRUE(engine->binding_stats("subscription").has_value());
  EXPECT_FALSE(engine->binding_stats("missing").has_value());
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
                                         .payload_type = 111,
                                         .transport = &transport},
                                        &error));

  ASSERT_TRUE(engine->RemoveBinding("subscription"));

  EXPECT_FALSE(engine->HasBinding("subscription"));
  EXPECT_EQ(engine->binding_count(), 0u);
  EXPECT_TRUE(engine->binding_stats("subscription").has_value());
  engine->Stop();
}

}  // namespace
}  // namespace webrtc::roq
