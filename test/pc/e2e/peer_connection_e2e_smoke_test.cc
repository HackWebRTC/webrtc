/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "call/simulated_network.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/fake_network.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/api/create_peerconnection_quality_test_fixture.h"
#include "test/pc/e2e/api/peerconnection_quality_test_fixture.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/network_emulation_manager.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

std::unique_ptr<rtc::NetworkManager> CreateFakeNetworkManager(
    std::vector<EmulatedEndpoint*> endpoints) {
  auto network_manager = absl::make_unique<rtc::FakeNetworkManager>();
  for (auto* endpoint : endpoints) {
    network_manager->AddInterface(
        rtc::SocketAddress(endpoint->GetPeerLocalAddress(), /*port=*/0));
  }
  return network_manager;
}

void PrintFrameCounters(const std::string& name,
                        const FrameCounters& counters) {
  RTC_LOG(INFO) << "[" << name << "] Captured    : " << counters.captured;
  RTC_LOG(INFO) << "[" << name << "] Pre encoded : " << counters.pre_encoded;
  RTC_LOG(INFO) << "[" << name << "] Encoded     : " << counters.encoded;
  RTC_LOG(INFO) << "[" << name << "] Received    : " << counters.received;
  RTC_LOG(INFO) << "[" << name << "] Rendered    : " << counters.rendered;
  RTC_LOG(INFO) << "[" << name << "] Dropped     : " << counters.dropped;
}

}  // namespace

TEST(PeerConnectionE2EQualityTestSmokeTest, RunWithEmulatedNetwork) {
  using Params = PeerConnectionE2EQualityTestFixture::Params;
  using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
  using InjectableComponents =
      PeerConnectionE2EQualityTestFixture::InjectableComponents;

  auto alice_params = absl::make_unique<Params>();
  VideoConfig alice_video_config(1280, 720, 30);
  alice_video_config.stream_label = "alice-video";

  alice_params->video_configs.push_back(alice_video_config);
  alice_params->audio_config = AudioConfig();

  auto bob_params = absl::make_unique<Params>();
  VideoConfig bob_video_config(1280, 720, 30);
  bob_video_config.stream_label = "bob-video";

  bob_params->video_configs.push_back(bob_video_config);
  bob_params->audio_config = AudioConfig();

  // Setup emulated network
  NetworkEmulationManager network_emulation_manager;

  EmulatedNetworkNode* alice_node =
      network_emulation_manager.CreateEmulatedNode(
          absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_emulation_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_emulation_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_emulation_manager.CreateRoute(alice_endpoint, {alice_node},
                                        bob_endpoint);
  network_emulation_manager.CreateRoute(bob_endpoint, {bob_node},
                                        alice_endpoint);

  rtc::Thread* alice_network_thread =
      network_emulation_manager.CreateNetworkThread({alice_endpoint});
  rtc::Thread* bob_network_thread =
      network_emulation_manager.CreateNetworkThread({bob_endpoint});

  // Setup components. We need to provide rtc::NetworkManager compatible with
  // emulated network layer.
  std::unique_ptr<rtc::NetworkManager> alice_network_manager =
      CreateFakeNetworkManager({alice_endpoint});
  auto alice_components = absl::make_unique<InjectableComponents>(
      alice_network_thread, alice_network_manager.get());
  std::unique_ptr<rtc::NetworkManager> bob_network_manager =
      CreateFakeNetworkManager({bob_endpoint});
  auto bob_components = absl::make_unique<InjectableComponents>(
      bob_network_thread, bob_network_manager.get());

  // Create analyzers.
  std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer =
      absl::make_unique<DefaultVideoQualityAnalyzer>();
  // This is only done for the sake of smoke testing. In general there should
  // be no need to explicitly pull data from analyzers after the run.
  auto* video_analyzer_ptr =
      static_cast<DefaultVideoQualityAnalyzer*>(video_quality_analyzer.get());

  std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer =
      absl::make_unique<DefaultAudioQualityAnalyzer>();

  auto fixture = CreatePeerConnectionE2EQualityTestFixture(
      "smoke_test", std::move(audio_quality_analyzer),
      std::move(video_quality_analyzer));
  fixture->Run(std::move(alice_components), std::move(alice_params),
               std::move(bob_components), std::move(bob_params),
               RunParams{TimeDelta::seconds(5)});

  PrintFrameCounters("Global", video_analyzer_ptr->GetGlobalCounters());
  for (auto stream_label : video_analyzer_ptr->GetKnownVideoStreams()) {
    FrameCounters stream_conters =
        video_analyzer_ptr->GetPerStreamCounters().at(stream_label);
    PrintFrameCounters(stream_label, stream_conters);
    // 150 = 30fps * 5s. On some devices pipeline can be too slow, so it can
    // happen, that frames will stuck in the middle, so we actually can't force
    // real constraints here, so lets just check, that at least 1 frame passed
    // whole pipeline.
    EXPECT_GE(stream_conters.captured, 150);
    EXPECT_GE(stream_conters.pre_encoded, 1);
    EXPECT_GE(stream_conters.encoded, 1);
    EXPECT_GE(stream_conters.received, 1);
    EXPECT_GE(stream_conters.decoded, 1);
    EXPECT_GE(stream_conters.rendered, 1);
  }
}

}  // namespace test
}  // namespace webrtc
