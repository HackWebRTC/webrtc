/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/wav_writer.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/wav_header.h"

namespace webrtc {

// We write 16-bit PCM WAV files.
static const WavFormat kWavFormat = kWavFormatPcm;
static const int kBytesPerSample = 2;

WavFile::WavFile(const std::string& filename, int sample_rate, int num_channels)
    : sample_rate_(sample_rate),
      num_channels_(num_channels),
      num_samples_(0),
      file_handle_(fopen(filename.c_str(), "wb")) {
  CHECK(file_handle_);
  CHECK(CheckWavParameters(num_channels_,
                           sample_rate_,
                           kWavFormat,
                           kBytesPerSample,
                           num_samples_));

  // Write a blank placeholder header, since we need to know the total number
  // of samples before we can fill in the real data.
  static const uint8_t blank_header[kWavHeaderSize] = {0};
  CHECK_EQ(1u, fwrite(blank_header, kWavHeaderSize, 1, file_handle_));
}

WavFile::~WavFile() {
  Close();
}

void WavFile::WriteSamples(const int16_t* samples, size_t num_samples) {
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert samples to little-endian when writing to WAV file"
#endif
  const size_t written =
      fwrite(samples, sizeof(*samples), num_samples, file_handle_);
  CHECK_EQ(num_samples, written);
  num_samples_ += static_cast<uint32_t>(written);
  CHECK(written <= std::numeric_limits<uint32_t>::max() ||
        num_samples_ >= written);  // detect uint32_t overflow
  CHECK(CheckWavParameters(num_channels_,
                           sample_rate_,
                           kWavFormat,
                           kBytesPerSample,
                           num_samples_));
}

void WavFile::WriteSamples(const float* samples, size_t num_samples) {
  static const size_t kChunksize = 4096 / sizeof(uint16_t);
  for (size_t i = 0; i < num_samples; i += kChunksize) {
    int16_t isamples[kChunksize];
    const size_t chunk = std::min(kChunksize, num_samples - i);
    RoundToInt16(samples + i, chunk, isamples);
    WriteSamples(isamples, chunk);
  }
}

void WavFile::Close() {
  CHECK_EQ(0, fseek(file_handle_, 0, SEEK_SET));
  uint8_t header[kWavHeaderSize];
  WriteWavHeader(header, num_channels_, sample_rate_, kWavFormat,
                 kBytesPerSample, num_samples_);
  CHECK_EQ(1u, fwrite(header, kWavHeaderSize, 1, file_handle_));
  CHECK_EQ(0, fclose(file_handle_));
  file_handle_ = NULL;
}

}  // namespace webrtc

rtc_WavFile* rtc_WavOpen(const char* filename,
                         int sample_rate,
                         int num_channels) {
  return reinterpret_cast<rtc_WavFile*>(
      new webrtc::WavFile(filename, sample_rate, num_channels));
}

void rtc_WavClose(rtc_WavFile* wf) {
  delete reinterpret_cast<webrtc::WavFile*>(wf);
}

void rtc_WavWriteSamples(rtc_WavFile* wf,
                         const float* samples,
                         size_t num_samples) {
  reinterpret_cast<webrtc::WavFile*>(wf)->WriteSamples(samples, num_samples);
}

int rtc_WavSampleRate(const rtc_WavFile* wf) {
  return reinterpret_cast<const webrtc::WavFile*>(wf)->sample_rate();
}

int rtc_WavNumChannels(const rtc_WavFile* wf) {
  return reinterpret_cast<const webrtc::WavFile*>(wf)->num_channels();
}

uint32_t rtc_WavNumSamples(const rtc_WavFile* wf) {
  return reinterpret_cast<const webrtc::WavFile*>(wf)->num_samples();
}
