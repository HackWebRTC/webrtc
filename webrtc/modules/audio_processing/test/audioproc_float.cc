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
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

DEFINE_string(dump, "", "The name of the debug dump file to read from.");
DEFINE_string(i, "", "The name of the input file to read from.");
DEFINE_string(i_rev, "", "The name of the reverse input file to read from.");
DEFINE_string(o, "out.wav", "Name of the output file to write to.");
DEFINE_string(o_rev,
              "out_rev.wav",
              "Name of the reverse output file to write to.");
DEFINE_int32(out_channels, 0, "Number of output channels. Defaults to input.");
DEFINE_int32(out_sample_rate, 0,
             "Output sample rate in Hz. Defaults to input.");
DEFINE_string(mic_positions, "",
    "Space delimited cartesian coordinates of microphones in meters. "
    "The coordinates of each point are contiguous. "
    "For a two element array: \"x1 y1 z1 x2 y2 z2\"");
DEFINE_double(target_angle_degrees, 90, "The azimuth of the target in radians");

DEFINE_bool(aec, false, "Enable echo cancellation.");
DEFINE_bool(agc, false, "Enable automatic gain control.");
DEFINE_bool(hpf, false, "Enable high-pass filtering.");
DEFINE_bool(ns, false, "Enable noise suppression.");
DEFINE_bool(ts, false, "Enable transient suppression.");
DEFINE_bool(bf, false, "Enable beamforming.");
DEFINE_bool(ie, false, "Enable intelligibility enhancer.");
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

// Returns a StreamConfig corresponding to wav_file if it's non-nullptr.
// Otherwise returns a default initialized StreamConfig.
StreamConfig MakeStreamConfig(const WavFile* wav_file) {
  if (wav_file) {
    return {wav_file->sample_rate(), wav_file->num_channels()};
  }
  return {};
}

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
  config.Set<Intelligibility>(new Intelligibility(FLAGS_ie || FLAGS_all));

  if (FLAGS_bf || FLAGS_all) {
    const size_t num_mics = in_file.num_channels();
    const std::vector<Point> array_geometry =
        ParseArrayGeometry(FLAGS_mic_positions, num_mics);
    RTC_CHECK_EQ(array_geometry.size(), num_mics);

    config.Set<Beamforming>(new Beamforming(
        true, array_geometry,
        SphericalPointf(DegreesToRadians(FLAGS_target_angle_degrees), 0.f,
                        1.f)));
  }

  rtc::scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(config));
  if (!FLAGS_dump.empty()) {
    RTC_CHECK_EQ(kNoErr,
                 ap->echo_cancellation()->Enable(FLAGS_aec || FLAGS_all));
  } else if (FLAGS_aec) {
    fprintf(stderr, "-aec requires a -dump file.\n");
    return -1;
  }
  bool process_reverse = !FLAGS_i_rev.empty();
  RTC_CHECK_EQ(kNoErr, ap->gain_control()->Enable(FLAGS_agc || FLAGS_all));
  RTC_CHECK_EQ(kNoErr,
               ap->gain_control()->set_mode(GainControl::kFixedDigital));
  RTC_CHECK_EQ(kNoErr, ap->high_pass_filter()->Enable(FLAGS_hpf || FLAGS_all));
  RTC_CHECK_EQ(kNoErr, ap->noise_suppression()->Enable(FLAGS_ns || FLAGS_all));
  if (FLAGS_ns_level != -1) {
    RTC_CHECK_EQ(kNoErr,
                 ap->noise_suppression()->set_level(
                     static_cast<NoiseSuppression::Level>(FLAGS_ns_level)));
  }
  ap->set_stream_key_pressed(FLAGS_ts);

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

  rtc::scoped_ptr<WavReader> in_rev_file;
  rtc::scoped_ptr<WavWriter> out_rev_file;
  rtc::scoped_ptr<ChannelBuffer<float>> in_rev_buf;
  rtc::scoped_ptr<ChannelBuffer<float>> out_rev_buf;
  std::vector<float> in_rev_interleaved;
  std::vector<float> out_rev_interleaved;
  if (process_reverse) {
    in_rev_file.reset(new WavReader(FLAGS_i_rev));
    out_rev_file.reset(new WavWriter(FLAGS_o_rev, in_rev_file->sample_rate(),
                                     in_rev_file->num_channels()));
    printf("In rev file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
           FLAGS_i_rev.c_str(), in_rev_file->num_channels(),
           in_rev_file->sample_rate());
    printf("Out rev file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
           FLAGS_o_rev.c_str(), out_rev_file->num_channels(),
           out_rev_file->sample_rate());
    in_rev_buf.reset(new ChannelBuffer<float>(
        rtc::CheckedDivExact(in_rev_file->sample_rate(), kChunksPerSecond),
        in_rev_file->num_channels()));
    in_rev_interleaved.resize(in_rev_buf->size());
    out_rev_buf.reset(new ChannelBuffer<float>(
        rtc::CheckedDivExact(out_rev_file->sample_rate(), kChunksPerSecond),
        out_rev_file->num_channels()));
    out_rev_interleaved.resize(out_rev_buf->size());
  }

  TickTime processing_start_time;
  TickInterval accumulated_time;
  int num_chunks = 0;

  const auto input_config = MakeStreamConfig(&in_file);
  const auto output_config = MakeStreamConfig(&out_file);
  const auto reverse_input_config = MakeStreamConfig(in_rev_file.get());
  const auto reverse_output_config = MakeStreamConfig(out_rev_file.get());

  while (in_file.ReadSamples(in_interleaved.size(),
                             &in_interleaved[0]) == in_interleaved.size()) {
    // Have logs display the file time rather than wallclock time.
    trace_to_stderr.SetTimeSeconds(num_chunks * 1.f / kChunksPerSecond);
    FloatS16ToFloat(&in_interleaved[0], in_interleaved.size(),
                    &in_interleaved[0]);
    Deinterleave(&in_interleaved[0], in_buf.num_frames(),
                 in_buf.num_channels(), in_buf.channels());
    if (process_reverse) {
      in_rev_file->ReadSamples(in_rev_interleaved.size(),
                               in_rev_interleaved.data());
      FloatS16ToFloat(in_rev_interleaved.data(), in_rev_interleaved.size(),
                      in_rev_interleaved.data());
      Deinterleave(in_rev_interleaved.data(), in_rev_buf->num_frames(),
                   in_rev_buf->num_channels(), in_rev_buf->channels());
    }

    if (FLAGS_perf) {
      processing_start_time = TickTime::Now();
    }
    RTC_CHECK_EQ(kNoErr, ap->ProcessStream(in_buf.channels(), input_config,
                                           output_config, out_buf.channels()));
    if (process_reverse) {
      RTC_CHECK_EQ(kNoErr, ap->ProcessReverseStream(
                               in_rev_buf->channels(), reverse_input_config,
                               reverse_output_config, out_rev_buf->channels()));
    }
    if (FLAGS_perf) {
      accumulated_time += TickTime::Now() - processing_start_time;
    }

    Interleave(out_buf.channels(), out_buf.num_frames(),
               out_buf.num_channels(), &out_interleaved[0]);
    FloatToFloatS16(&out_interleaved[0], out_interleaved.size(),
                    &out_interleaved[0]);
    out_file.WriteSamples(&out_interleaved[0], out_interleaved.size());
    if (process_reverse) {
      Interleave(out_rev_buf->channels(), out_rev_buf->num_frames(),
                 out_rev_buf->num_channels(), out_rev_interleaved.data());
      FloatToFloatS16(out_rev_interleaved.data(), out_rev_interleaved.size(),
                      out_rev_interleaved.data());
      out_rev_file->WriteSamples(out_rev_interleaved.data(),
                                 out_rev_interleaved.size());
    }
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
