/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/session_description.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {

TEST(RemoteEstimateEndToEnd, OfferedCapabilityIsInAnswer) {
  PeerScenario s;

  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  auto send_link = {s.net()->NodeBuilder().Build().node};
  auto ret_link = {s.net()->NodeBuilder().Build().node};

  s.net()->CreateRoute(caller->endpoint(), send_link, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), ret_link, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, send_link, ret_link);
  caller->CreateVideo("VIDEO", PeerScenarioClient::VideoSendTrackConfig());
  rtc::Event offer_exchange_done;
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* offer) {
        for (auto& cont : offer->description()->contents()) {
          cont.media_description()->set_remote_estimate(true);
        }
      },
      [&](const SessionDescriptionInterface& answer) {
        for (auto& cont : answer.description()->contents()) {
          EXPECT_TRUE(cont.media_description()->remote_estimate());
        }
        offer_exchange_done.Set();
      });
  EXPECT_TRUE(s.WaitAndProcess(&offer_exchange_done));
}

}  // namespace test
}  // namespace webrtc
