/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/test/field_trial.h"
#include "webrtc/test/run_test.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/video_quality_test.h"

namespace webrtc {

namespace flags {

DEFINE_int32(width, 640, "Video width.");
size_t Width() {
  return static_cast<size_t>(FLAGS_width);
}

DEFINE_int32(height, 480, "Video height.");
size_t Height() {
  return static_cast<size_t>(FLAGS_height);
}

DEFINE_int32(fps, 30, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(min_bitrate, 50, "Call and stream min bitrate in kbps.");
int MinBitrateKbps() {
  return static_cast<int>(FLAGS_min_bitrate);
}

DEFINE_int32(start_bitrate, 300, "Call start bitrate in kbps.");
int StartBitrateKbps() {
  return static_cast<int>(FLAGS_start_bitrate);
}

DEFINE_int32(target_bitrate, 800, "Stream target bitrate in kbps.");
int TargetBitrateKbps() {
  return static_cast<int>(FLAGS_target_bitrate);
}

DEFINE_int32(max_bitrate, 800, "Call and stream max bitrate in kbps.");
int MaxBitrateKbps() {
  return static_cast<int>(FLAGS_max_bitrate);
}

DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAGS_codec);
}

DEFINE_int32(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAGS_loss_percent);
}

DEFINE_int32(link_capacity,
             0,
             "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacityKbps() {
  return static_cast<int>(FLAGS_link_capacity);
}

DEFINE_int32(queue_size, 0, "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAGS_queue_size);
}

DEFINE_int32(avg_propagation_delay_ms,
             0,
             "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAGS_avg_propagation_delay_ms);
}

DEFINE_int32(std_propagation_delay_ms,
             0,
             "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAGS_std_propagation_delay_ms);
}

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

DEFINE_int32(num_temporal_layers,
             1,
             "Number of temporal layers. Set to 1-4 to override.");
size_t NumTemporalLayers() {
  return static_cast<size_t>(FLAGS_num_temporal_layers);
}

DEFINE_int32(
    tl_discard_threshold,
    0,
    "Discard TLs with id greater or equal the threshold. 0 to disable.");
size_t TLDiscardThreshold() {
  return static_cast<size_t>(FLAGS_tl_discard_threshold);
}

DEFINE_string(clip,
              "",
              "Name of the clip to show. If empty, using chroma generator.");
std::string Clip() {
  return static_cast<std::string>(FLAGS_clip);
}

DEFINE_string(
    output_filename,
    "",
    "Name of a target graph data file. If set, no preview will be shown.");
std::string OutputFilename() {
  return static_cast<std::string>(FLAGS_output_filename);
}

DEFINE_int32(duration, 60, "Duration of the test in seconds.");
int DurationSecs() {
  return static_cast<int>(FLAGS_duration);
}

DEFINE_bool(send_side_bwe, true, "Use send-side bandwidth estimation");

}  // namespace flags

void Loopback() {
  FakeNetworkPipe::Config pipe_config;
  pipe_config.loss_percent = flags::LossPercent();
  pipe_config.link_capacity_kbps = flags::LinkCapacityKbps();
  pipe_config.queue_length_packets = flags::QueueSize();
  pipe_config.queue_delay_ms = flags::AvgPropagationDelayMs();
  pipe_config.delay_standard_deviation_ms = flags::StdPropagationDelayMs();

  Call::Config::BitrateConfig call_bitrate_config;
  call_bitrate_config.min_bitrate_bps = flags::MinBitrateKbps() * 1000;
  call_bitrate_config.start_bitrate_bps = flags::StartBitrateKbps() * 1000;
  call_bitrate_config.max_bitrate_bps = flags::MaxBitrateKbps() * 1000;

  std::string clip = flags::Clip();
  std::string graph_title = clip.empty() ? "" : "video " + clip;
  VideoQualityTest::Params params{
      {flags::Width(), flags::Height(), flags::Fps(),
       flags::MinBitrateKbps() * 1000, flags::TargetBitrateKbps() * 1000,
       flags::MaxBitrateKbps() * 1000, flags::Codec(),
       flags::NumTemporalLayers(),
       0,  // No min transmit bitrate.
       call_bitrate_config, flags::TLDiscardThreshold(),
       flags::FLAGS_send_side_bwe},
      {clip},
      {},  // Screenshare specific.
      {graph_title, 0.0, 0.0, flags::DurationSecs(), flags::OutputFilename()},
      pipe_config,
      flags::FLAGS_logs};

  VideoQualityTest test;
  if (flags::OutputFilename().empty())
    test.RunWithVideoRenderer(params);
  else
    test.RunWithAnalyzer(params);
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::InitFieldTrialsFromString(
      webrtc::flags::FLAGS_force_fieldtrials);
  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
