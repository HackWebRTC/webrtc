/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on the WAV file format documentation at
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/ and
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#include "webrtc/common_audio/wav_header.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "webrtc/common_audio/include/audio_util.h"

namespace webrtc {

struct ChunkHeader {
  uint32_t ID;
  uint32_t Size;
};
COMPILE_ASSERT(sizeof(ChunkHeader) == 8, chunk_header_size);

bool CheckWavParameters(int num_channels,
                        int sample_rate,
                        WavFormat format,
                        int bytes_per_sample,
                        uint32_t num_samples) {
  // num_channels, sample_rate, and bytes_per_sample must be positive, must fit
  // in their respective fields, and their product must fit in the 32-bit
  // ByteRate field.
  if (num_channels <= 0 || sample_rate <= 0 || bytes_per_sample <= 0)
    return false;
  if (static_cast<uint64_t>(sample_rate) > std::numeric_limits<uint32_t>::max())
    return false;
  if (static_cast<uint64_t>(num_channels) >
      std::numeric_limits<uint16_t>::max())
    return false;
  if (static_cast<uint64_t>(bytes_per_sample) * 8 >
      std::numeric_limits<uint16_t>::max())
    return false;
  if (static_cast<uint64_t>(sample_rate) * num_channels * bytes_per_sample >
      std::numeric_limits<uint32_t>::max())
    return false;

  // format and bytes_per_sample must agree.
  switch (format) {
    case kWavFormatPcm:
      // Other values may be OK, but for now we're conservative:
      if (bytes_per_sample != 1 && bytes_per_sample != 2)
        return false;
      break;
    case kWavFormatALaw:
    case kWavFormatMuLaw:
      if (bytes_per_sample != 1)
        return false;
      break;
    default:
      return false;
  }

  // The number of bytes in the file, not counting the first ChunkHeader, must
  // be less than 2^32; otherwise, the ChunkSize field overflows.
  const uint32_t max_samples =
      (std::numeric_limits<uint32_t>::max()
       - (kWavHeaderSize - sizeof(ChunkHeader))) /
      bytes_per_sample;
  if (num_samples > max_samples)
    return false;

  // Each channel must have the same number of samples.
  if (num_samples % num_channels != 0)
    return false;

  return true;
}

#ifdef WEBRTC_ARCH_LITTLE_ENDIAN
static inline void WriteLE16(uint16_t* f, uint16_t x) { *f = x; }
static inline void WriteLE32(uint32_t* f, uint32_t x) { *f = x; }
static inline void WriteFourCC(uint32_t* f, char a, char b, char c, char d) {
  *f = static_cast<uint32_t>(a)
      | static_cast<uint32_t>(b) << 8
      | static_cast<uint32_t>(c) << 16
      | static_cast<uint32_t>(d) << 24;
}
#else
#error "Write be-to-le conversion functions"
#endif

void WriteWavHeader(uint8_t* buf,
                    int num_channels,
                    int sample_rate,
                    WavFormat format,
                    int bytes_per_sample,
                    uint32_t num_samples) {
  assert(CheckWavParameters(num_channels, sample_rate, format,
                            bytes_per_sample, num_samples));

  struct {
    struct {
      ChunkHeader header;
      uint32_t Format;
    } riff;
    struct {
      ChunkHeader header;
      uint16_t AudioFormat;
      uint16_t NumChannels;
      uint32_t SampleRate;
      uint32_t ByteRate;
      uint16_t BlockAlign;
      uint16_t BitsPerSample;
    } fmt;
    struct {
      ChunkHeader header;
    } data;
  } header;
  COMPILE_ASSERT(sizeof(header) == kWavHeaderSize, no_padding_in_header);

  const uint32_t bytes_in_payload = bytes_per_sample * num_samples;

  WriteFourCC(&header.riff.header.ID, 'R', 'I', 'F', 'F');
  WriteLE32(&header.riff.header.Size,
            bytes_in_payload + kWavHeaderSize - sizeof(ChunkHeader));
  WriteFourCC(&header.riff.Format, 'W', 'A', 'V', 'E');

  WriteFourCC(&header.fmt.header.ID, 'f', 'm', 't', ' ');
  WriteLE32(&header.fmt.header.Size, sizeof(header.fmt) - sizeof(ChunkHeader));
  WriteLE16(&header.fmt.AudioFormat, format);
  WriteLE16(&header.fmt.NumChannels, num_channels);
  WriteLE32(&header.fmt.SampleRate, sample_rate);
  WriteLE32(&header.fmt.ByteRate, (static_cast<uint32_t>(num_channels)
                                   * sample_rate * bytes_per_sample));
  WriteLE16(&header.fmt.BlockAlign, num_channels * bytes_per_sample);
  WriteLE16(&header.fmt.BitsPerSample, 8 * bytes_per_sample);

  WriteFourCC(&header.data.header.ID, 'd', 'a', 't', 'a');
  WriteLE32(&header.data.header.Size, bytes_in_payload);

  // Do an extra copy rather than writing everything to buf directly, since buf
  // might not be correctly aligned.
  memcpy(buf, &header, kWavHeaderSize);
}

}  // namespace webrtc
