/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/protobuf_utils.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

DEFINE_string(dump, "", "The name of the debug dump file to read from.");
DEFINE_string(i, "", "The name of the input file to read from.");
DEFINE_string(o, "out.wav", "Name of the output file to write to.");
DEFINE_int32(out_channels, 0, "Number of output channels. Defaults to input.");
DEFINE_int32(out_sample_rate, 0,
             "Output sample rate in Hz. Defaults to input.");
DEFINE_string(mic_positions, "",
    "Space delimited cartesian coordinates of microphones in meters. "
    "The coordinates of each point are contiguous. "
    "For a two element array: \"x1 y1 z1 x2 y2 z2\"");

DEFINE_bool(aec, false, "Enable echo cancellation.");
DEFINE_bool(agc, false, "Enable automatic gain control.");
DEFINE_bool(hpf, false, "Enable high-pass filtering.");
DEFINE_bool(ns, false, "Enable noise suppression.");
DEFINE_bool(ts, false, "Enable transient suppression.");
DEFINE_bool(bf, false, "Enable beamforming.");
DEFINE_bool(all, false, "Enable all components.");

DEFINE_int32(ns_level, -1, "Noise suppression level [0 - 3].");

DEFINE_bool(perf, false, "Enable performance tests.");

namespace webrtc {
namespace {

const int kChunksPerSecond = 100;
const char kUsage[] =
    "Command-line tool to run audio processing on WAV files. Accepts either\n"
    "an input capture WAV file or protobuf debug dump and writes to an output\n"
    "WAV file.\n"
    "\n"
    "All components are disabled by default. If any bi-directional components\n"
    "are enabled, only debug dump files are permitted.";

}  // namespace

int main(int argc, char* argv[]) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!((FLAGS_i.empty()) ^ (FLAGS_dump.empty()))) {
    fprintf(stderr,
            "An input file must be specified with either -i or -dump.\n");
    return 1;
  }
  if (!FLAGS_dump.empty()) {
    fprintf(stderr, "FIXME: the -dump option is not yet implemented.\n");
    return 1;
  }

  test::TraceToStderr trace_to_stderr(true);
  WavReader in_file(FLAGS_i);
  // If the output format is uninitialized, use the input format.
  const int out_channels =
      FLAGS_out_channels ? FLAGS_out_channels : in_file.num_channels();
  const int out_sample_rate =
      FLAGS_out_sample_rate ? FLAGS_out_sample_rate : in_file.sample_rate();
  WavWriter out_file(FLAGS_o, out_sample_rate, out_channels);

  Config config;
  config.Set<ExperimentalNs>(new ExperimentalNs(FLAGS_ts || FLAGS_all));

  if (FLAGS_bf || FLAGS_all) {
    const size_t num_mics = in_file.num_channels();
    const std::vector<Point> array_geometry =
        ParseArrayGeometry(FLAGS_mic_positions, num_mics);
    CHECK_EQ(array_geometry.size(), num_mics);

    config.Set<Beamforming>(new Beamforming(true, array_geometry));
  }

  rtc::scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(config));
  if (!FLAGS_dump.empty()) {
    CHECK_EQ(kNoErr, ap->echo_cancellation()->Enable(FLAGS_aec || FLAGS_all));
  } else if (FLAGS_aec) {
    fprintf(stderr, "-aec requires a -dump file.\n");
    return -1;
  }
  CHECK_EQ(kNoErr, ap->gain_control()->Enable(FLAGS_agc || FLAGS_all));
  CHECK_EQ(kNoErr, ap->gain_control()->set_mode(GainControl::kFixedDigital));
  CHECK_EQ(kNoErr, ap->high_pass_filter()->Enable(FLAGS_hpf || FLAGS_all));
  CHECK_EQ(kNoErr, ap->noise_suppression()->Enable(FLAGS_ns || FLAGS_all));
  if (FLAGS_ns_level != -1)
    CHECK_EQ(kNoErr, ap->noise_suppression()->set_level(
        static_cast<NoiseSuppression::Level>(FLAGS_ns_level)));

  printf("Input file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
         FLAGS_i.c_str(), in_file.num_channels(), in_file.sample_rate());
  printf("Output file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
         FLAGS_o.c_str(), out_file.num_channels(), out_file.sample_rate());

  ChannelBuffer<float> in_buf(
      rtc::CheckedDivExact(in_file.sample_rate(), kChunksPerSecond),
      in_file.num_channels());
  ChannelBuffer<float> out_buf(
      rtc::CheckedDivExact(out_file.sample_rate(), kChunksPerSecond),
      out_file.num_channels());

  std::vector<float> in_interleaved(in_buf.size());
  std::vector<float> out_interleaved(out_buf.size());
  TickTime processing_start_time;
  TickInterval accumulated_time;
  int num_chunks = 0;
  while (in_file.ReadSamples(in_interleaved.size(),
                             &in_interleaved[0]) == in_interleaved.size()) {
    // Have logs display the file time rather than wallclock time.
    trace_to_stderr.SetTimeSeconds(num_chunks * 1.f / kChunksPerSecond);
    FloatS16ToFloat(&in_interleaved[0], in_interleaved.size(),
                    &in_interleaved[0]);
    Deinterleave(&in_interleaved[0], in_buf.num_frames(),
                 in_buf.num_channels(), in_buf.channels());

    if (FLAGS_perf) {
      processing_start_time = TickTime::Now();
    }
    CHECK_EQ(kNoErr,
        ap->ProcessStream(in_buf.channels(),
                          in_buf.num_frames(),
                          in_file.sample_rate(),
                          LayoutFromChannels(in_buf.num_channels()),
                          out_file.sample_rate(),
                          LayoutFromChannels(out_buf.num_channels()),
                          out_buf.channels()));
    if (FLAGS_perf) {
      accumulated_time += TickTime::Now() - processing_start_time;
    }

    Interleave(out_buf.channels(), out_buf.num_frames(),
               out_buf.num_channels(), &out_interleaved[0]);
    FloatToFloatS16(&out_interleaved[0], out_interleaved.size(),
                    &out_interleaved[0]);
    out_file.WriteSamples(&out_interleaved[0], out_interleaved.size());
    num_chunks++;
  }
  if (FLAGS_perf) {
    int64_t execution_time_ms = accumulated_time.Milliseconds();
    printf("\nExecution time: %.3f s\nFile time: %.2f s\n"
           "Time per chunk: %.3f ms\n",
           execution_time_ms * 0.001f, num_chunks * 1.f / kChunksPerSecond,
           execution_time_ms * 1.f / num_chunks);
  }
  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::main(argc, argv);
}
