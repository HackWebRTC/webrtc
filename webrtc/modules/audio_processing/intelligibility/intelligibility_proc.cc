/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Command line tool for speech intelligibility enhancement. Provides for
//  running and testing intelligibility_enhancer as an independent process.
//  Use --help for options.
//

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fenv.h>
#include <limits>

#include <complex>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_audio/real_fourier.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

// PCM data simulating streams
const int16_t* in_ipcm;
int16_t* out_ipcm;
const int16_t* noise_ipcm;
float* in_fpcm;
float* out_fpcm;
float* noise_fpcm;

// Current locations in streams
float* noise_cursor;
float* clear_cursor;

int samples;        // Number of samples in input PCM file
int fragment_size;  // Number of samples to process at a time
                    // to simulate APM stream processing

using std::complex;

namespace webrtc {

using webrtc::RealFourier;
using webrtc::IntelligibilityEnhancer;

DEFINE_int32(clear_type,
             webrtc::intelligibility::VarianceArray::kStepInfinite,
             "Variance algorithm for clear data.");
DEFINE_double(clear_alpha, 0.9, "Variance decay factor for clear data.");
DEFINE_int32(clear_window,
             475,
             "Window size for windowed variance for clear data.");
DEFINE_int32(sample_rate,
             16000,
             "Audio sample rate used in the input and output files.");
DEFINE_int32(ana_rate,
             800,
             "Analysis rate; gains recalculated every N blocks.");
DEFINE_int32(
    var_rate,
    2,
    "Variance clear rate; history is forgotten every N gain recalculations.");
DEFINE_double(gain_limit, 1000.0, "Maximum gain change in one block.");

DEFINE_bool(repeat, false, "Repeat input file ad nauseam.");

DEFINE_string(clear_file, "speech.pcm", "Input file with clear speech.");
DEFINE_string(noise_file, "noise.pcm", "Input file with noise data.");
DEFINE_string(out_file,
              "proc_enhanced.pcm",
              "Enhanced output. Use '-' to "
              "pipe through aplay internally.");

// Constant IntelligibilityEnhancer constructor parameters.
const int kErbResolution = 2;
const int kNumChannels = 1;

// Converts output stream to Sun AU format and writes to file descriptor |fd|.
// Can be used to pipe output directly into aplay.
// TODO(ekmeyerson): Modify to write WAV instead.
void writeau(int fd) {
  uint32_t thing;

  write(fd, ".snd", 4);
  thing = htonl(24);
  write(fd, &thing, sizeof(thing));
  thing = htonl(0xffffffff);
  write(fd, &thing, sizeof(thing));
  thing = htonl(3);
  write(fd, &thing, sizeof(thing));
  thing = htonl(FLAGS_sample_rate);
  write(fd, &thing, sizeof(thing));
  thing = htonl(1);
  write(fd, &thing, sizeof(thing));

  for (int i = 0; i < samples; ++i) {
    out_ipcm[i] = htons(out_ipcm[i]);
  }
  write(fd, out_ipcm, sizeof(*out_ipcm) * samples);
}

// void function for gtest
void void_main(int argc, char* argv[]) {
  google::SetUsageMessage(
      "\n\nVariance algorithm types are:\n"
      "  0 - infinite/normal,\n"
      "  1 - exponentially decaying,\n"
      "  2 - rolling window.\n"
      "\nInput files must be little-endian 16-bit signed raw PCM.\n");
  google::ParseCommandLineFlags(&argc, &argv, true);

  const char* in_name = FLAGS_clear_file.c_str();
  const char* out_name = FLAGS_out_file.c_str();
  const char* noise_name = FLAGS_noise_file.c_str();
  struct stat in_stat, noise_stat;
  int in_fd, out_fd, noise_fd;
  FILE* aplay_file = nullptr;

  // Load settings and set up PCMs.

  fragment_size = FLAGS_sample_rate / 100;  // Mirror real time APM chunk size.
                                            // Duplicates chunk_length_ in
                                            // IntelligibilityEnhancer.

  ASSERT_EQ(stat(in_name, &in_stat), 0) << "Empty speech input.";
  ASSERT_EQ(stat(noise_name, &noise_stat), 0) << "Empty noise input.";

  samples = in_stat.st_size / sizeof(*in_ipcm);

  in_fd = open(in_name, O_RDONLY);
  if (!strcmp(out_name, "-")) {
    aplay_file = popen("aplay -t au", "w");
    out_fd = fileno(aplay_file);
  } else {
    out_fd = open(out_name, O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  }
  noise_fd = open(noise_name, O_RDONLY);

  in_ipcm = static_cast<int16_t*>(
      mmap(nullptr, in_stat.st_size, PROT_READ, MAP_PRIVATE, in_fd, 0));
  noise_ipcm = static_cast<int16_t*>(
      mmap(nullptr, noise_stat.st_size, PROT_READ, MAP_PRIVATE, noise_fd, 0));
  out_ipcm = new int16_t[samples];
  out_fpcm = new float[samples];
  in_fpcm = new float[samples];
  noise_fpcm = new float[samples];

  for (int i = 0; i < samples; ++i) {
    noise_fpcm[i] = noise_ipcm[i % (noise_stat.st_size / sizeof(*noise_ipcm))];
  }

  // Run intelligibility enhancement.

  IntelligibilityEnhancer enh(
      kErbResolution,
      FLAGS_sample_rate,
      kNumChannels,
      FLAGS_clear_type, static_cast<float>(FLAGS_clear_alpha),
      FLAGS_clear_window, FLAGS_ana_rate, FLAGS_var_rate, FLAGS_gain_limit);

  // Slice the input into smaller chunks, as the APM would do, and feed them
  // through the enhancer. Repeat indefinitely if FLAGS_repeat is set.
  do {
    noise_cursor = noise_fpcm;
    clear_cursor = in_fpcm;
    for (int i = 0; i < samples; ++i) {
      in_fpcm[i] = in_ipcm[i];
    }

    for (int i = 0; i < samples; i += fragment_size) {
      enh.ProcessCaptureAudio(&noise_cursor);
      enh.ProcessRenderAudio(&clear_cursor);
      clear_cursor += fragment_size;
      noise_cursor += fragment_size;
    }

    for (int i = 0; i < samples; ++i) {
      out_ipcm[i] = static_cast<float>(in_fpcm[i]);
    }
    if (!strcmp(out_name, "-")) {
      writeau(out_fd);
    } else {
      write(out_fd, out_ipcm, samples * sizeof(*out_ipcm));
    }
  } while (FLAGS_repeat);

  munmap(const_cast<int16_t*>(noise_ipcm), noise_stat.st_size);
  munmap(const_cast<int16_t*>(in_ipcm), in_stat.st_size);
  close(noise_fd);
  if (aplay_file) {
    pclose(aplay_file);
  } else {
    close(out_fd);
  }
  close(in_fd);
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::void_main(argc, argv);
  return 0;
}
