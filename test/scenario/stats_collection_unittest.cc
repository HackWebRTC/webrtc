/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/stats_collection.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
void CreateAnalyzedStream(Scenario* s,
                          NetworkNodeConfig network_config,
                          VideoQualityAnalyzer* analyzer) {
  VideoStreamConfig config;
  config.encoder.codec = VideoStreamConfig::Encoder::Codec::kVideoCodecVP8;
  config.encoder.implementation =
      VideoStreamConfig::Encoder::Implementation::kSoftware;
  config.hooks.frame_pair_handlers = {analyzer->Handler()};
  auto route = s->CreateRoutes(s->CreateClient("caller", CallClientConfig()),
                               {s->CreateSimulationNode(network_config)},
                               s->CreateClient("callee", CallClientConfig()),
                               {s->CreateSimulationNode(NetworkNodeConfig())});
  s->CreateVideoStream(route->forward(), config);
}
}  // namespace

TEST(ScenarioAnalyzerTest, PsnrIsHighWhenNetworkIsGood) {
  VideoQualityAnalyzer analyzer;
  {
    Scenario s("", /*real_time*/ false);
    NetworkNodeConfig good_network;
    good_network.simulation.bandwidth = DataRate::kbps(1000);
    CreateAnalyzedStream(&s, good_network, &analyzer);
    s.RunFor(TimeDelta::seconds(1));
  }
  // This is mainty a regression test, the target is based on previous runs and
  // might change due to changes in configuration and encoder etc.
  EXPECT_GT(analyzer.stats().psnr.Mean(), 40);
}

TEST(ScenarioAnalyzerTest, PsnrIsLowWhenNetworkIsBad) {
  VideoQualityAnalyzer analyzer;
  {
    Scenario s("", /*real_time*/ false);
    NetworkNodeConfig bad_network;
    bad_network.simulation.bandwidth = DataRate::kbps(100);
    bad_network.simulation.loss_rate = 0.02;
    CreateAnalyzedStream(&s, bad_network, &analyzer);
    s.RunFor(TimeDelta::seconds(1));
  }
  // This is mainty a regression test, the target is based on previous runs and
  // might change due to changes in configuration and encoder etc.
  EXPECT_LT(analyzer.stats().psnr.Mean(), 30);
}
}  // namespace test
}  // namespace webrtc
