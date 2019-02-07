/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/api/create_peerconnection_quality_test_fixture.h"

#include <utility>

#include "absl/memory/memory.h"
#include "test/pc/e2e/peer_connection_quality_test.h"

namespace webrtc {

std::unique_ptr<PeerConnectionE2EQualityTestFixture>
CreatePeerConnectionE2EQualityTestFixture(
    std::unique_ptr<PeerConnectionE2EQualityTestFixture::InjectableComponents>
        alice_components,
    std::unique_ptr<PeerConnectionE2EQualityTestFixture::Params> alice_params,
    std::unique_ptr<PeerConnectionE2EQualityTestFixture::InjectableComponents>
        bob_components,
    std::unique_ptr<PeerConnectionE2EQualityTestFixture::Params> bob_params,
    std::unique_ptr<PeerConnectionE2EQualityTestFixture::Analyzers> analyzers) {
  return absl::make_unique<webrtc::test::PeerConnectionE2EQualityTest>(
      std::move(alice_components), std::move(alice_params),
      std::move(bob_components), std::move(bob_params), std::move(analyzers));
}

}  // namespace webrtc
