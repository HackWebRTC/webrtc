/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/memory/memory.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/test/simulated_network.h"
#include "call/simulated_network.h"
#include "rtc_base/flags.h"
#include "test/gtest.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

using PeerConfigurer =
    webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::PeerConfigurer;
using RunParams = webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::RunParams;
using AudioConfig =
    webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::AudioConfig;

namespace {

constexpr int kTestDurationSec = 45;

EmulatedNetworkNode* CreateEmulatedNodeWithConfig(
    NetworkEmulationManager* emulation,
    const BuiltInNetworkBehaviorConfig& config) {
  return emulation->CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(config));
}

std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
CreateTwoNetworkLinks(NetworkEmulationManager* emulation,
                      const BuiltInNetworkBehaviorConfig& config) {
  auto* alice_node = CreateEmulatedNodeWithConfig(emulation, config);
  auto* bob_node = CreateEmulatedNodeWithConfig(emulation, config);

  auto* alice_endpoint = emulation->CreateEndpoint(EmulatedEndpointConfig());
  auto* bob_endpoint = emulation->CreateEndpoint(EmulatedEndpointConfig());

  emulation->CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  emulation->CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  return {
      emulation->CreateEmulatedNetworkManagerInterface({alice_endpoint}),
      emulation->CreateEmulatedNetworkManagerInterface({bob_endpoint}),
  };
}

std::unique_ptr<webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture>
CreateTestFixture(const std::string& test_case_name,
                  std::pair<EmulatedNetworkManagerInterface*,
                            EmulatedNetworkManagerInterface*> network_links,
                  rtc::FunctionView<void(PeerConfigurer*)> alice_configurer,
                  rtc::FunctionView<void(PeerConfigurer*)> bob_configurer) {
  auto fixture = webrtc_pc_e2e::CreatePeerConnectionE2EQualityTestFixture(
      test_case_name, /*audio_quality_analyzer=*/nullptr,
      /*video_quality_analyzer=*/nullptr);
  fixture->AddPeer(network_links.first->network_thread(),
                   network_links.first->network_manager(), alice_configurer);
  fixture->AddPeer(network_links.second->network_thread(),
                   network_links.second->network_manager(), bob_configurer);
  fixture->AddQualityMetricsReporter(
      absl::make_unique<webrtc_pc_e2e::NetworkQualityMetricsReporter>(
          network_links.first, network_links.second));
  return fixture;
}

std::string AudioInputFile() {
  return test::ResourcePath("voice_engine/audio_tiny48", "wav");
}

std::string AudioOutputFile() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return webrtc::test::OutputPath() + "LowBandwidth_" + test_info->name() +
         "_48.wav";
}

void PrintTestInfo() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();

  // Output information about the input and output audio files so that further
  // processing can be done by an external process.
  printf("TEST %s %s %s\n", test_info->name(), AudioInputFile().c_str(),
         AudioOutputFile().c_str());
}

}  // namespace

TEST(PCLowBandwidthAudioTest, GoodNetworkHighBitrate) {
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();
  auto fixture = CreateTestFixture(
      "pc_good_network",
      CreateTwoNetworkLinks(network_emulation_manager.get(),
                            BuiltInNetworkBehaviorConfig()),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name = AudioInputFile();
        audio.output_dump_file_name = AudioOutputFile();
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {});
  fixture->Run(RunParams(TimeDelta::seconds(kTestDurationSec)));
  PrintTestInfo();
}

TEST(PCLowBandwidthAudioTest, Mobile2GNetwork) {
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();
  BuiltInNetworkBehaviorConfig config;
  config.link_capacity_kbps = 12;
  config.queue_length_packets = 1500;
  config.queue_delay_ms = 400;
  auto fixture = CreateTestFixture(
      "pc_mobile_2g_network",
      CreateTwoNetworkLinks(network_emulation_manager.get(), config),
      [](PeerConfigurer* alice) {
        AudioConfig audio;
        audio.stream_label = "alice-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name = AudioInputFile();
        audio.output_dump_file_name = AudioOutputFile();
        alice->SetAudioConfig(std::move(audio));
      },
      [](PeerConfigurer* bob) {});
  RunParams run_params(TimeDelta::seconds(kTestDurationSec));
  fixture->Run(RunParams(TimeDelta::seconds(kTestDurationSec)));
  PrintTestInfo();
}

}  // namespace test
}  // namespace webrtc
