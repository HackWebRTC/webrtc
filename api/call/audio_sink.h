/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CALL_AUDIO_SINK_H_
#define API_CALL_AUDIO_SINK_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

// Per-stream NetEq observations, before mixer gain/panning and resampling.
struct AudioPlayoutQuality {
  uint64_t total_samples = 0;
  uint64_t concealed_samples = 0;
  uint64_t silent_concealed_samples = 0;
  uint64_t concealment_events = 0;
  uint64_t interruption_count = 0;
  uint64_t interruption_duration_us = 0;
  uint64_t max_interruption_duration_us = 0;
  uint64_t current_concealment_duration_us = 0;
  uint64_t output_duration_us = 0;
};

// Represents a simple push audio sink.
class AudioSinkInterface {
 public:
  virtual ~AudioSinkInterface() {}

  struct Data {
    Data(const int16_t* data,
         size_t samples_per_channel,
         int sample_rate,
         size_t channels,
         uint32_t timestamp,
         bool decoded_from_packet = true,
         AudioPlayoutQuality quality = {})
        : data(data),
          samples_per_channel(samples_per_channel),
          sample_rate(sample_rate),
          channels(channels),
          timestamp(timestamp),
          decoded_from_packet(decoded_from_packet),
          quality(quality) {}

    const int16_t* data;         // The actual 16bit audio data.
    size_t samples_per_channel;  // Number of frames in the buffer.
    int sample_rate;             // Sample rate in Hz.
    size_t channels;             // Number of channels in the audio data.
    uint32_t timestamp;          // The RTP timestamp of the first sample.
    // A successful speech decode this pull, including RED/FEC recovery.
    // False for PLC, CNG, startup silence and buffered-only output.
    bool decoded_from_packet;
    AudioPlayoutQuality quality;
  };

  virtual void OnData(const Data& audio) = 0;
};

}  // namespace webrtc

#endif  // API_CALL_AUDIO_SINK_H_
