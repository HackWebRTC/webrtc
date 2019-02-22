/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <atomic>

#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
using Capture = VideoStreamConfig::Source::Capture;
using ContentType = VideoStreamConfig::Encoder::ContentType;
using Codec = VideoStreamConfig::Encoder::Codec;
using CodecImpl = VideoStreamConfig::Encoder::Implementation;
}  // namespace

// TODO(srte): Enable this after resolving flakiness issues.
TEST(VideoStreamTest, DISABLED_ReceivesFramesFromFileBasedStreams) {
  TimeDelta kRunTime = TimeDelta::ms(500);
  std::vector<int> kFrameRates = {15, 30};
  std::deque<std::atomic<int>> frame_counts(2);
  frame_counts[0] = 0;
  frame_counts[1] = 0;
  {
    Scenario s;
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});

    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_counts[0]++;
      };
      c->source.capture = Capture::kVideoFile;
      c->source.video_file.name = "foreman_cif";
      c->source.video_file.width = 352;
      c->source.video_file.height = 288;
      c->source.framerate = kFrameRates[0];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
    });
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_counts[1]++;
      };
      c->source.capture = Capture::kImageSlides;
      c->source.slides.images.crop.width = 320;
      c->source.slides.images.crop.height = 240;
      c->source.framerate = kFrameRates[1];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP9;
    });
    s.RunFor(kRunTime);
  }
  std::vector<int> expected_counts;
  for (int fps : kFrameRates)
    expected_counts.push_back(
        static_cast<int>(kRunTime.seconds<double>() * fps * 0.8));

  EXPECT_GE(frame_counts[0], expected_counts[0]);
  EXPECT_GE(frame_counts[1], expected_counts[1]);
}

// TODO(srte): Enable this after resolving flakiness issues.
TEST(VideoStreamTest, DISABLED_RecievesVp8SimulcastFrames) {
  TimeDelta kRunTime = TimeDelta::ms(500);
  int kFrameRate = 30;

  std::atomic<int> frame_count(0);
  {
    Scenario s;
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      // TODO(srte): Replace with code checking for all simulcast streams when
      // there's a hook available for that.
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_count++;
      };
      c->source.framerate = kFrameRate;
      // The resolution must be high enough to allow smaller layers to be
      // created.
      c->source.generator.width = 1024;
      c->source.generator.height = 768;

      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
      // By enabling multiple spatial layers, simulcast will be enabled for VP8.
      c->encoder.layers.spatial = 3;
    });
    s.RunFor(kRunTime);
  }

  // Using 20% error margin to avoid flakyness.
  const int kExpectedCount =
      static_cast<int>(kRunTime.seconds<double>() * kFrameRate * 0.8);

  EXPECT_GE(frame_count, kExpectedCount);
}
}  // namespace test
}  // namespace webrtc
