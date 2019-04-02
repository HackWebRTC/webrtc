/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_H_
#define TEST_PC_E2E_TEST_PEER_H_

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "media/base/media_engine.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Describes a single participant in the call.
class TestPeer final : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;

  // Setups all components, that should be provided to WebRTC
  // PeerConnectionFactory and PeerConnection creation methods,
  // also will setup dependencies, that are required for media analyzers
  // injection.
  //
  // |signaling_thread| will be provided by test fixture implementation.
  // |params| - describes current peer paramters, like current peer video
  // streams and audio streams
  // |audio_outpu_file_name| - the name of output file, where incoming audio
  // stream should be written. It should be provided from remote peer
  // |params.audio_config.output_file_name|
  static std::unique_ptr<TestPeer> CreateTestPeer(
      std::unique_ptr<InjectableComponents> components,
      std::unique_ptr<Params> params,
      std::unique_ptr<MockPeerConnectionObserver> observer,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* signaling_thread,
      absl::optional<std::string> audio_output_file_name,
      double bitrate_multiplier);

  Params* params() const { return params_.get(); }

  // Adds provided |candidates| to the owned peer connection.
  bool AddIceCandidates(
      rtc::ArrayView<const IceCandidateInterface* const> candidates);

 private:
  TestPeer(rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
           rtc::scoped_refptr<PeerConnectionInterface> pc,
           std::unique_ptr<MockPeerConnectionObserver> observer,
           std::unique_ptr<Params> params);

  std::unique_ptr<Params> params_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_H_
