/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_FACTORY_H_
#define TEST_PC_E2E_TEST_PEER_FACTORY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/task_queue.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/peer_configurer.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

struct RemotePeerAudioConfig {
  explicit RemotePeerAudioConfig(
      PeerConnectionE2EQualityTestFixture::AudioConfig config)
      : sampling_frequency_in_hz(config.sampling_frequency_in_hz),
        output_file_name(config.output_dump_file_name) {}

  static absl::optional<RemotePeerAudioConfig> Create(
      absl::optional<PeerConnectionE2EQualityTestFixture::AudioConfig> config);

  int sampling_frequency_in_hz;
  absl::optional<std::string> output_file_name;
};

class TestPeerFactory {
 public:
  // Setups all components, that should be provided to WebRTC
  // PeerConnectionFactory and PeerConnection creation methods,
  // also will setup dependencies, that are required for media analyzers
  // injection.
  //
  // |signaling_thread| will be provided by test fixture implementation.
  // |params| - describes current peer parameters, like current peer video
  // streams and audio streams
  static std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<InjectableComponents> components,
      std::unique_ptr<Params> params,
      std::vector<PeerConfigurerImpl::VideoSource> video_sources,
      std::unique_ptr<MockPeerConnectionObserver> observer,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* signaling_thread,
      absl::optional<RemotePeerAudioConfig> remote_audio_config,
      double bitrate_multiplier,
      absl::optional<PeerConnectionE2EQualityTestFixture::EchoEmulationConfig>
          echo_emulation_config,
      rtc::TaskQueue* task_queue);
  // Setups all components, that should be provided to WebRTC
  // PeerConnectionFactory and PeerConnection creation methods,
  // also will setup dependencies, that are required for media analyzers
  // injection.
  //
  // |signaling_thread| will be provided by test fixture implementation.
  static std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<PeerConfigurerImpl> configurer,
      std::unique_ptr<MockPeerConnectionObserver> observer,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* signaling_thread,
      absl::optional<RemotePeerAudioConfig> remote_audio_config,
      double bitrate_multiplier,
      absl::optional<PeerConnectionE2EQualityTestFixture::EchoEmulationConfig>
          echo_emulation_config,
      rtc::TaskQueue* task_queue);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_FACTORY_H_
