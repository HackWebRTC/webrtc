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
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "call/simulated_network.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {

TEST(PeerConnectionE2EQualityTestSmokeTest, RunWithEmulatedNetwork) {
  using PeerConfigurer = PeerConnectionE2EQualityTestFixture::PeerConfigurer;
  using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;

  // Setup emulated network
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();

  auto alice_network_behavior =
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig());
  SimulatedNetwork* alice_network_behavior_ptr = alice_network_behavior.get();
  EmulatedNetworkNode* alice_node =
      network_emulation_manager->CreateEmulatedNode(
          std::move(alice_network_behavior));
  EmulatedNetworkNode* bob_node = network_emulation_manager->CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
  network_emulation_manager->CreateRoute(alice_endpoint, {alice_node},
                                         bob_endpoint);
  network_emulation_manager->CreateRoute(bob_endpoint, {bob_node},
                                         alice_endpoint);

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
  fixture->ExecuteAt(TimeDelta::seconds(2),
                     [alice_network_behavior_ptr](TimeDelta) {
                       BuiltInNetworkBehaviorConfig config;
                       config.loss_percent = 5;
                       alice_network_behavior_ptr->SetConfig(config);
                     });

  // Setup components. We need to provide rtc::NetworkManager compatible with
  // emulated network layer.
  fixture->AddPeer(
      network_emulation_manager->CreateNetworkThread({alice_endpoint}),
      network_emulation_manager->CreateNetworkManager({alice_endpoint}),
      [](PeerConfigurer* alice) {
        VideoConfig alice_video_config(640, 360, 30);
        alice_video_config.stream_label = "alice-video";
        alice->AddVideoConfig(std::move(alice_video_config));
        alice->SetAudioConfig(AudioConfig());
      });

  fixture->AddPeer(
      network_emulation_manager->CreateNetworkThread({bob_endpoint}),
      network_emulation_manager->CreateNetworkManager({bob_endpoint}),
      [](PeerConfigurer* bob) {
        VideoConfig bob_video_config(640, 360, 30);
        bob_video_config.stream_label = "bob-video";
        bob->AddVideoConfig(std::move(bob_video_config));
        bob->SetAudioConfig(AudioConfig());
      });

  fixture->Run(RunParams{TimeDelta::seconds(5)});

  for (auto stream_label : video_analyzer_ptr->GetKnownVideoStreams()) {
    FrameCounters stream_conters =
        video_analyzer_ptr->GetPerStreamCounters().at(stream_label);
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

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
